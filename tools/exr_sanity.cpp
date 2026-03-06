#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <Imath/ImathBox.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ImageRGB {
  int width = 0;
  int height = 0;
  int xmin = 0;
  int ymin = 0;
  std::vector<float> r;
  std::vector<float> g;
  std::vector<float> b;
};

static bool channel_exists(const Imf::ChannelList& channels, const char* name) {
  for (Imf::ChannelList::ConstIterator it = channels.begin(); it != channels.end(); ++it) {
    if (std::string(it.name()) == name) return true;
  }
  return false;
}

static ImageRGB read_exr_rgb(const std::string& path) {
  Imf::InputFile file(path.c_str());
  const Imf::Header& header = file.header();
  const Imf::ChannelList& channels = header.channels();

  if (!channel_exists(channels, "R") || !channel_exists(channels, "G") || !channel_exists(channels, "B")) {
    std::cerr << "exr-sanity: expected channels R/G/B in " << path << "\n";
    std::exit(2);
  }

  const Imath::Box2i dw = header.dataWindow();
  const int xmin = dw.min.x;
  const int ymin = dw.min.y;
  const int xmax = dw.max.x;
  const int ymax = dw.max.y;

  ImageRGB img;
  img.xmin = xmin;
  img.ymin = ymin;
  img.width = (xmax - xmin + 1);
  img.height = (ymax - ymin + 1);
  const std::size_t n = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);
  img.r.assign(n, 0.0f);
  img.g.assign(n, 0.0f);
  img.b.assign(n, 0.0f);

  Imf::FrameBuffer fb;
  const std::size_t xStride = sizeof(float);
  const std::size_t yStride = sizeof(float) * static_cast<std::size_t>(img.width);

  float* rBase = img.r.data() - xmin - static_cast<std::ptrdiff_t>(ymin) * img.width;
  float* gBase = img.g.data() - xmin - static_cast<std::ptrdiff_t>(ymin) * img.width;
  float* bBase = img.b.data() - xmin - static_cast<std::ptrdiff_t>(ymin) * img.width;

  fb.insert("R", Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(rBase), xStride, yStride, 1, 1, 0.0f));
  fb.insert("G", Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(gBase), xStride, yStride, 1, 1, 0.0f));
  fb.insert("B", Imf::Slice(Imf::FLOAT, reinterpret_cast<char*>(bBase), xStride, yStride, 1, 1, 0.0f));

  file.setFrameBuffer(fb);
  file.readPixels(ymin, ymax);

  return img;
}

static inline float luminance_rec709(float r, float g, float b) {
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

struct Stats {
  float minR = std::numeric_limits<float>::infinity();
  float minG = std::numeric_limits<float>::infinity();
  float minB = std::numeric_limits<float>::infinity();
  float maxR = 0.0f;
  float maxG = 0.0f;
  float maxB = 0.0f;

  float minY = std::numeric_limits<float>::infinity();
  float maxY = 0.0f;
  double sumY = 0.0;
  std::size_t nonzeroCount = 0;

  double comX = 0.0;
  double comY = 0.0;
  double energy = 0.0;
  double sigmaR = 0.0;
  double r50 = 0.0;
  double r90 = 0.0;
  double r99 = 0.0;
};

static Stats compute_stats(const ImageRGB& img, float eps) {
  Stats s;

  // Pass 1: min/max/sum and COM.
  double sumW = 0.0;
  double sumX = 0.0;
  double sumY = 0.0;

  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * img.width + x;
      const float r = img.r[idx];
      const float g = img.g[idx];
      const float b = img.b[idx];
      const float Y = luminance_rec709(r, g, b);

      s.minR = std::min(s.minR, r);
      s.minG = std::min(s.minG, g);
      s.minB = std::min(s.minB, b);
      s.maxR = std::max(s.maxR, r);
      s.maxG = std::max(s.maxG, g);
      s.maxB = std::max(s.maxB, b);

      s.minY = std::min(s.minY, Y);
      s.maxY = std::max(s.maxY, Y);
      s.sumY += static_cast<double>(Y);

      if (Y > eps) {
        s.nonzeroCount++;
        sumW += static_cast<double>(Y);
        sumX += static_cast<double>(x) * static_cast<double>(Y);
        sumY += static_cast<double>(y) * static_cast<double>(Y);
      }
    }
  }

  if (sumW <= 0.0) {
    s.energy = 0.0;
    s.comX = (img.width - 1) * 0.5;
    s.comY = (img.height - 1) * 0.5;
    return s;
  }

  s.energy = sumW;
  s.comX = sumX / sumW;
  s.comY = sumY / sumW;

  // Pass 2: radial energy distribution + sigma estimate.
  std::vector<std::pair<float, float>> radial;  // (r, Y)
  radial.reserve(s.nonzeroCount);

  double sumR2 = 0.0;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * img.width + x;
      const float Y = luminance_rec709(img.r[idx], img.g[idx], img.b[idx]);
      if (Y <= eps) continue;
      const double dx = static_cast<double>(x) - s.comX;
      const double dy = static_cast<double>(y) - s.comY;
      const double r = std::sqrt(dx * dx + dy * dy);
      radial.emplace_back(static_cast<float>(r), Y);
      sumR2 += (dx * dx + dy * dy) * static_cast<double>(Y);
    }
  }
  s.sigmaR = std::sqrt(sumR2 / sumW);

  std::sort(radial.begin(), radial.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  const double e50 = 0.50 * sumW;
  const double e90 = 0.90 * sumW;
  const double e99 = 0.99 * sumW;
  double accum = 0.0;

  auto pick_r = [&](double target) -> double {
    if (radial.empty()) return 0.0;
    accum = 0.0;
    for (const auto& [r, Y] : radial) {
      accum += static_cast<double>(Y);
      if (accum >= target) return static_cast<double>(r);
    }
    return static_cast<double>(radial.back().first);
  };

  s.r50 = pick_r(e50);
  s.r90 = pick_r(e90);
  s.r99 = pick_r(e99);

  return s;
}

static float parse_eps_arg(int argc, char** argv) {
  // Optional: --eps <float>
  for (int i = 2; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--eps") return std::strtof(argv[i + 1], nullptr);
  }
  return -1.0f;
}

static bool has_flag(int argc, char** argv, const char* flag) {
  for (int i = 2; i < argc; ++i) {
    if (std::string(argv[i]) == flag) return true;
  }
  return false;
}

static std::string json_escape(const std::string& s) {
  std::ostringstream oss;
  for (char c : s) {
    switch (c) {
      case '\\': oss << "\\\\"; break;
      case '"': oss << "\\\""; break;
      case '\n': oss << "\\n"; break;
      case '\r': oss << "\\r"; break;
      case '\t': oss << "\\t"; break;
      default: oss << c; break;
    }
  }
  return oss.str();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: bin/exr-sanity <image.exr> [--eps <float>] [--json]\n";
    return 2;
  }

  const std::string path = argv[1];
  const bool asJson = has_flag(argc, argv, "--json");
  const ImageRGB img = read_exr_rgb(path);

  const float userEps = parse_eps_arg(argc, argv);
  const float maxYGuess = std::max({img.r.empty() ? 0.0f : *std::max_element(img.r.begin(), img.r.end()),
                                    img.g.empty() ? 0.0f : *std::max_element(img.g.begin(), img.g.end()),
                                    img.b.empty() ? 0.0f : *std::max_element(img.b.begin(), img.b.end())});
  const float eps = (userEps > 0.0f) ? userEps : std::max(1e-8f, maxYGuess * 1e-6f);

  const Stats s = compute_stats(img, eps);

  const std::size_t total = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);
  const double meanY = s.sumY / static_cast<double>(total);

  std::cout << std::fixed << std::setprecision(6);
  if (asJson) {
    std::cout << "{";
    std::cout << "\"file\":\"" << json_escape(path) << "\",";
    std::cout << "\"width\":" << img.width << ",";
    std::cout << "\"height\":" << img.height << ",";
    std::cout << "\"xmin\":" << img.xmin << ",";
    std::cout << "\"ymin\":" << img.ymin << ",";
    std::cout << "\"minRGB\":[" << s.minR << "," << s.minG << "," << s.minB << "],";
    std::cout << "\"maxRGB\":[" << s.maxR << "," << s.maxG << "," << s.maxB << "],";
    std::cout << "\"minY\":" << s.minY << ",";
    std::cout << "\"maxY\":" << s.maxY << ",";
    std::cout << "\"meanY\":" << meanY << ",";
    std::cout << "\"eps\":" << eps << ",";
    std::cout << "\"nonzeroCount\":" << s.nonzeroCount << ",";
    std::cout << "\"totalCount\":" << total << ",";
    std::cout << "\"energy\":" << s.energy << ",";
    std::cout << "\"comX\":" << s.comX << ",";
    std::cout << "\"comY\":" << s.comY << ",";
    std::cout << "\"sigmaR\":" << s.sigmaR << ",";
    std::cout << "\"r50\":" << s.r50 << ",";
    std::cout << "\"r90\":" << s.r90 << ",";
    std::cout << "\"r99\":" << s.r99;
    std::cout << "}\n";
  } else {
    std::cout << "file: " << path << "\n";
    std::cout << "size: " << img.width << " x " << img.height << " (dataWindow origin " << img.xmin << "," << img.ymin
              << ")\n";
    std::cout << "minRGB: (" << s.minR << ", " << s.minG << ", " << s.minB << ")\n";
    std::cout << "maxRGB: (" << s.maxR << ", " << s.maxG << ", " << s.maxB << ")\n";
    std::cout << "minY/maxY/meanY: " << s.minY << " / " << s.maxY << " / " << meanY << "\n";
    std::cout << "eps: " << eps << "\n";
    std::cout << "nonzero(Y>eps): " << s.nonzeroCount << " / " << total << " ("
              << (100.0 * static_cast<double>(s.nonzeroCount) / static_cast<double>(total)) << "%)\n";
    std::cout << "energy(Y>eps): " << s.energy << "\n";
    std::cout << "center_of_mass(x,y): (" << s.comX << ", " << s.comY << ")\n";
    std::cout << "sigma_radius(px): " << s.sigmaR << "\n";
    std::cout << "encircled_radius_px (50/90/99%): " << s.r50 << " / " << s.r90 << " / " << s.r99 << "\n";
  }
  return 0;
}
