/* Polynomial Optics
 *
 * Example_EclipsedBokeh.cpp
 *
 * Minimal reproduction of “eclipsed bokeh” (Debevec 2020): a point light’s
 * out-of-focus bokeh disk becomes sharply clipped by an out-of-focus occluder.
 *
 * This demo:
 *  - chooses a focus distance (sets film plane once)
 *  - renders a background point light at another distance (defocused)
 *  - places an opaque disk occluder at an intermediate distance and moves it
 *    along +Y (top-to-bottom in the rendered image coordinate system)
 *  - optionally renders the occluder center as a faint bokeh disk for reference
 *
 * Output: a sequence of EXR frames (and optional PNG).
 */

#include <TruncPoly/TruncPolySystem.hh>

#include <OpticalElements/Cylindrical5.hh>
#include <OpticalElements/FindFocus.hh>
#include <OpticalElements/OpticalMaterial.hh>
#include <OpticalElements/Propagation5.hh>
#include <OpticalElements/Spherical5.hh>
#include <OpticalElements/TwoPlane5.hh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // getopt

#define cimg_display 0
#include <include/CImg.h>
#include <include/spectrum.h>

using namespace std;
using namespace cimg_library;

struct Light2D {
  float x = 0.0f;
  float y = 0.0f;
  float amp = 1.0f;
};

static void showUsage(const char *exe) {
  cout << "\nEclipsed Bokeh — Polynomial Optics demo\n\n";
  cout << "Usage: " << exe << " [options] [output_prefix]\n\n";
  cout << "Core options:\n";
  cout << "  -i <systems/*.lens>   Lens definition (optional; default: achromat)\n";
  cout << "  -c <degree>           Truncation degree (default: 3)\n";
  cout << "  -F <mm>               Focus distance (default: 1000)\n";
  cout << "  -E <mm>               Focus distance end (default: = -F)\n";
  cout << "  -d <mm>               Film defocus offset (default: 0)\n";
  cout << "  -D <mm>               Film defocus end (default: = -d)\n";
  cout << "  -L <mm>               Light distance (default: 2000)\n";
  cout << "  -M <mm>               Light distance end (default: = -L)\n";
  cout << "  -K <mm>               Occluder distance (default: 1500)\n";
  cout << "  -N <mm>               Occluder distance end (default: = -K)\n";
  cout << "  -T <mm>               Target (emitter) radius in light plane (default: 0 = point)\n";
  cout << "  -Q <count>            Background light count (default: 1)\n";
  cout << "  -U <mm>               Light position spread (mm; square, centered at 0) (default: 120)\n";
  cout << "  -V <seed>             Light layout seed (default: 1)\n";
  cout << "  -J <mm>               Light min separation (mm) (default: 0)\n";
  cout << "  -G <samples>          Samples per light (overrides -S) (default: 0)\n";
  cout << "  -R <mm>               Occluder radius (default: 15)\n";
  cout << "  -X <mm>               Occluder X position (default: 0)\n";
  cout << "  -A <mm>               Occluder Y-from (default: 40)\n";
  cout << "  -B <mm>               Occluder Y-to (default: -40)\n";
  cout << "  -n <frames>           Frame count (default: 41)\n";
  cout << "  -S <samples>          Aperture samples per bokeh (default: 200000)\n";
  cout << "  -P <mm>               Aperture radius (default: 19.5)\n";
  cout << "  -w <mm>               Sensor width (default: 80)\n";
  cout << "  -x <px>               X resolution (default: 1024)\n";
  cout << "  -y <px>               Y resolution (default: 1024)\n";
  cout << "  -o <prefix>           Output prefix (default: OutputEXR/eclipsed-bokeh/eclipsed)\n";
  cout << "  -g <gain>             Light radiance gain (default: 500)\n";
  cout << "  -u                    Also write baseline (no occluder) + diff (blocked energy)\n";
  cout << "  -W <count>            Wavelength sample count (default: 1)\n";
  cout << "  -a <nm>               Wavelength range start (default: 440)\n";
  cout << "  -b <nm>               Wavelength range end (default: 660)\n";
  cout << "  -C <nm>               Focus reference wavelength (default: 550)\n";
  cout << "  -p                    Also write PNG (tonemapped)\n";
  cout << "  -q <exposure>         PNG tonemap exposure (default: 1)\n";
  cout << "  -m <0|1|2|3>          Occluder reference:\n";
  cout << "                         0=off, 1=overlay bokeh, 2=separate bokeh EXR, 3=overlay marker (default: 0)\n";
  cout << "  -O <rel>              Overlay occluder outline (dashed red), value = rel * frame max (default: 0 = off)\n";
  cout << "  -H <rel>              Overlay light depth labels at bokeh centers, value = rel * frame max (default: 0 = off)\n";
  cout << "  -s <seed>             RNG seed (default: 1)\n";
  cout << "\nCases (set distances):\n";
  cout << "  -t A|B|C              Debevec cases (overrides -F/-L/-K)\n";
  cout << "    A: F < K < L (focus in front of both)\n";
  cout << "    B: K < F < L (occluder closer than focus)\n";
  cout << "    C: K < L < F (focus behind light)\n\n";
  cout << "Distance ramps (over frames):\n";
  cout << "  Provide both start and end values (e.g. -L 1500 -M 6000) to linearly\n";
  cout << "  interpolate distances across frames and make bokeh changes easier to see.\n\n";
  cout << "Notes:\n";
  cout << "  You may also provide output_prefix as the last positional argument.\n\n";
}

static inline bool inBounds(const CImg<float> &img, int x, int y) {
  return x >= 0 && y >= 0 && x < img.width() && y < img.height();
}

static void drawDashedProjectedOccluderOutline(CImg<float> &img,
                                               Transform4f &system_occ_ref,
                                               float sensor_scaling,
                                               float x_occ,
                                               float y_occ,
                                               float r_occ,
                                               float red_value) {
  if (red_value <= 0.0f || r_occ <= 0.0f) return;

  // Project the geometric occluder boundary using the chief ray (xp=yp=0).
  // This is a visualization aid, not a physical image of the occluder.
  static const int kSegments = 256;
  float u[kSegments];
  float v[kSegments];
  float in[4];
  float out[4];

  const float two_pi = 6.2831853071795864769f;
  for (int i = 0; i < kSegments; ++i) {
    const float t = (float)i / (float)kSegments * two_pi;
    const float xw = x_occ + r_occ * cosf(t);
    const float yw = y_occ + r_occ * sinf(t);
    in[0] = xw;
    in[1] = yw;
    in[2] = 0.0f;
    in[3] = 0.0f;
    system_occ_ref.evaluate(in, out);
    u[i] = out[0] * sensor_scaling + img.width() * 0.5f;
    v[i] = out[1] * sensor_scaling + img.height() * 0.5f;
  }

  const float dash_px = 10.0f;
  const float gap_px = 7.0f;
  const float period = dash_px + gap_px;
  float acc = 0.0f;

  for (int i = 0; i < kSegments; ++i) {
    const int j = (i + 1) % kSegments;
    const float x0 = u[i], y0 = v[i];
    const float x1 = u[j], y1 = v[j];
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len <= 1e-6f) continue;

    // Step ~0.6px for a dotted feel without too much cost.
    const int steps = max(1, (int)ceilf(len / 0.6f));
    for (int s = 0; s <= steps; ++s) {
      const float ts = (float)s / (float)steps;
      const float x = x0 + dx * ts;
      const float y = y0 + dy * ts;
      const float d = len * ts;
      const float phase = fmodf(acc + d, period);
      if (phase > dash_px) continue;

      const int xi = (int)lroundf(x);
      const int yi = (int)lroundf(y);
      if (!inBounds(img, xi, yi)) continue;

      img(xi, yi, 0, 0) = max(img(xi, yi, 0, 0), red_value);
    }
    acc += len;
  }
}

static void drawLightDepthLabels(CImg<float> &img,
                                 Transform4f &system_light_ref,
                                 float sensor_scaling,
                                 const vector<Light2D> &lights,
                                 float z_light_mm,
                                 float label_value) {
  if (label_value <= 0.0f) return;
  if (lights.empty()) return;

  const float color[3] = {label_value, 0.20f * label_value, 0.20f * label_value};
  const float opacity = 0.9f;

  char text[64];
  snprintf(text, sizeof(text), "%.0fmm", z_light_mm);

  float in[4];
  float out[4];
  for (const auto &L : lights) {
    in[0] = L.x;
    in[1] = L.y;
    in[2] = 0.0f;
    in[3] = 0.0f;
    system_light_ref.evaluate(in, out);
    const int cx = (int)lroundf(out[0] * sensor_scaling + img.width() * 0.5f);
    const int cy = (int)lroundf(out[1] * sensor_scaling + img.height() * 0.5f);
    if (!inBounds(img, cx, cy)) continue;

    // Slight offset to avoid covering the brightest bokeh core.
    const int x = min(max(0, cx + 6), img.width() - 1);
    const int y = min(max(0, cy + 6), img.height() - 1);
    img.draw_text(x, y, text, color, (const float *)nullptr, opacity, 13);
  }
}

static Transform4f identity4(int degree) {
  Transform4f t;
  t.trunc_degree = degree;
  for (int i = 0; i < 4; ++i) {
    Poly4f p;
    p.trunc_degree = degree;
    PolyTerm<float, 4> term(1.0f);
    term.exponents[0] = 0;
    term.exponents[1] = 0;
    term.exponents[2] = 0;
    term.exponents[3] = 0;
    term.exponents[i] = 1;
    p.terms.push_back(term);
    p.consolidated = false;
    p.consolidate_terms();
    t[i] = p;
  }
  t.consolidate_terms();
  return t;
}

static bool parseGlassOrIndex(const string &token, float lambda_nm, float &out_n) {
  if (token.empty()) return false;
  if ((token[0] >= '0' && token[0] <= '9') || token[0] == '.' || token[0] == '-') {
    out_n = static_cast<float>(atof(token.c_str()));
    return true;
  }
  OpticalMaterial glass(token.c_str());
  out_n = glass.get_index(lambda_nm);
  return true;
}

static Transform4f lens_only_default(float lambda_nm, int degree) {
  OpticalMaterial glass1("N-SSK8", true);
  OpticalMaterial glass2("N-SF10", true);

  const float R1 = 65.22f;
  const float d1 = 9.60f;
  const float R2 = -62.03f;
  const float d2 = 4.20f;
  const float R3 = -1240.67f;

  return refract_spherical_5(R1, 1.f, glass1.get_index(lambda_nm), degree) >>
         propagate_5(d1, degree) >>
         refract_spherical_5(R2, glass1.get_index(lambda_nm), glass2.get_index(lambda_nm), degree) >>
         propagate_5(d2, degree) >>
         refract_spherical_5(R3, glass2.get_index(lambda_nm), 1.f, degree);
}

static Transform4f lens_only_from_file(const char *filename, float lambda_nm, int degree) {
  ifstream infile(filename);
  if (!infile) {
    cerr << "lens file not found: " << filename << endl;
    exit(1);
  }

  Transform4f system = identity4(degree);
  string line;
  while (getline(infile, line)) {
    if (line.empty()) continue;
    if (line[0] == '#') continue;
    istringstream ls(line);
    string op;
    ls >> op;
    if (op.empty()) continue;

    if (op == "two_plane") {
      continue; // ignored here; focus/world distance handled separately
    } else if (op == "propagate") {
      float d;
      ls >> d;
      system = system >> propagate_5(d, degree);
    } else if (op == "reflect_spherical") {
      float radius;
      ls >> radius;
      system = system >> reflect_spherical_5(radius, degree);
    } else if (op == "refract_spherical") {
      float radius;
      string a, b;
      ls >> radius >> a >> b;
      float n1 = 1.f, n2 = 1.f;
      parseGlassOrIndex(a, lambda_nm, n1);
      parseGlassOrIndex(b, lambda_nm, n2);
      system = system >> refract_spherical_5(radius, n1, n2, degree);
    } else if (op == "cylindrical_x") {
      float radius;
      string a, b;
      ls >> radius >> a >> b;
      float n1 = 1.f, n2 = 1.f;
      parseGlassOrIndex(a, lambda_nm, n1);
      parseGlassOrIndex(b, lambda_nm, n2);
      system = system >> refract_cylindrical_x_5(radius, n1, n2, degree);
    } else if (op == "cylindrical_y") {
      float radius;
      string a, b;
      ls >> radius >> a >> b;
      float n1 = 1.f, n2 = 1.f;
      parseGlassOrIndex(a, lambda_nm, n1);
      parseGlassOrIndex(b, lambda_nm, n2);
      system = system >> refract_cylindrical_y_5(radius, n1, n2, degree);
    } else {
      cerr << "invalid op in lens file: " << op << endl;
      exit(1);
    }
  }
  return system;
}

static inline float rand01() { return rand() / (float)RAND_MAX; }

static bool mkdir_p(const string &path) {
  if (path.empty()) return true;
  if (path == "/") return true;

  string tmp;
  tmp.reserve(path.size());

  for (size_t i = 0; i < path.size(); ++i) {
    const char c = path[i];
    tmp.push_back(c);
    if (c != '/' && i + 1 != path.size()) continue;
    if (tmp.size() == 1 && tmp[0] == '/') continue;
    if (mkdir(tmp.c_str(), 0755) != 0 && errno != EEXIST) return false;
  }
  if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) return false;
  return true;
}

static void ensureOutDirForPrefix(const string &out_prefix) {
  const size_t slash = out_prefix.find_last_of('/');
  if (slash == string::npos) return;
  const string dir = out_prefix.substr(0, slash);
  if (!mkdir_p(dir)) {
    cerr << "failed to create output directory: " << dir << endl;
    exit(1);
  }
}

static void sampleApertureDisk(float r, float &x, float &y) {
  while (true) {
    x = (rand01() * 2.0f - 1.0f) * r;
    y = (rand01() * 2.0f - 1.0f) * r;
    if (x * x + y * y <= r * r) return;
  }
}

static bool rayHitsOccluderDisk(float xw, float yw, float zw_mm, float xp, float yp, float zocc_mm,
                                float xocc, float yocc, float rocc) {
  if (zocc_mm <= 0 || zw_mm <= 0) return false;
  if (zocc_mm >= zw_mm) return false;
  const float f = zocc_mm / zw_mm;
  const float xi = xp + f * (xw - xp);
  const float yi = yp + f * (yw - yp);
  const float dx = xi - xocc;
  const float dy = yi - yocc;
  return (dx * dx + dy * dy) <= (rocc * rocc);
}

static void splatBokehPoint(CImg<float> &img, Transform4f &system_plane, float sensor_scaling,
                            float xw, float yw, float target_radius_world, float r_pupil, int samples, float r,
                            float g, float b, bool occlude, float z_world, float z_occ, float xocc, float yocc,
                            float rocc) {
  float in[4];
  float out[4];
  for (int s = 0; s < samples; ++s) {
    float xw_s = xw;
    float yw_s = yw;
    if (target_radius_world > 0.0f) {
      float dx, dy;
      sampleApertureDisk(target_radius_world, dx, dy);
      xw_s += dx;
      yw_s += dy;
    }

    float xp, yp;
    sampleApertureDisk(r_pupil, xp, yp);
    if (occlude && rayHitsOccluderDisk(xw_s, yw_s, z_world, xp, yp, z_occ, xocc, yocc, rocc)) continue;
    in[0] = xw_s;
    in[1] = yw_s;
    in[2] = xp;
    in[3] = yp;
    system_plane.evaluate(in, out);
    const float u = out[0] * sensor_scaling + img.width() * 0.5f;
    const float v = out[1] * sensor_scaling + img.height() * 0.5f;
    const float w = 1.0f / samples;
    img.set_linear_atXY(r * w, u, v, 0, 0, true);
    img.set_linear_atXY(g * w, u, v, 0, 1, true);
    img.set_linear_atXY(b * w, u, v, 0, 2, true);
  }
}

static void tonemapToSRGB(const CImg<float> &hdr, CImg<unsigned char> &ldr, float exposure) {
  const int w = hdr.width();
  const int h = hdr.height();
  ldr.assign(w, h, 1, 3, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        float v = hdr(x, y, 0, c) * exposure;
        v = v / (1.0f + v); // simple Reinhard
        v = powf(max(0.0f, v), 1.0f / 2.2f);
        ldr(x, y, 0, c) = (unsigned char)max(0, min(255, (int)lroundf(v * 255.0f)));
      }
    }
  }
}

static inline float lerpFloat(float a, float b, float t) { return a + (b - a) * t; }

static void drawCrosshair(CImg<float> &img, int cx, int cy, int half_len, float r, float g, float b) {
  const int w = img.width();
  const int h = img.height();
  for (int dx = -half_len; dx <= half_len; ++dx) {
    const int x = cx + dx;
    if (x < 0 || x >= w || cy < 0 || cy >= h) continue;
    img(x, cy, 0, 0) = max(img(x, cy, 0, 0), r);
    img(x, cy, 0, 1) = max(img(x, cy, 0, 1), g);
    img(x, cy, 0, 2) = max(img(x, cy, 0, 2), b);
  }
  for (int dy = -half_len; dy <= half_len; ++dy) {
    const int y = cy + dy;
    if (cx < 0 || cx >= w || y < 0 || y >= h) continue;
    img(cx, y, 0, 0) = max(img(cx, y, 0, 0), r);
    img(cx, y, 0, 1) = max(img(cx, y, 0, 1), g);
    img(cx, y, 0, 2) = max(img(cx, y, 0, 2), b);
  }
}

static void subtractClamp(const CImg<float> &a, const CImg<float> &b, CImg<float> &out) {
  const int w = a.width();
  const int h = a.height();
  out.assign(w, h, 1, 3, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        out(x, y, 0, c) = max(0.0f, a(x, y, 0, c) - b(x, y, 0, c));
      }
    }
  }
}

static void clampNonNegative(CImg<float> &img) {
  const int w = img.width();
  const int h = img.height();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        img(x, y, 0, c) = max(0.0f, img(x, y, 0, c));
      }
    }
  }
}

static inline uint32_t xorshift32(uint32_t &state) {
  uint32_t x = state ? state : 1u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

static inline float rand01_u32(uint32_t &state) { return xorshift32(state) / 4294967295.0f; }

int main(int argc, char **argv) {
  if (argc == 1) {
    showUsage(argv[0]);
    return 0;
  }

  int degree = 3;
  float z_focus = 1000.0f;
  float z_focus_end = 1000.0f;
  bool z_focus_end_set = false;
  float film_defocus = 0.0f;
  float film_defocus_end = 0.0f;
  bool film_defocus_end_set = false;
  float z_light = 2000.0f;
  float z_light_end = 2000.0f;
  bool z_light_end_set = false;
  float z_occ = 1500.0f;
  float z_occ_end = 1500.0f;
  bool z_occ_end_set = false;
  float target_radius = 0.0f;
  int light_count = 1;
  float light_spread = 120.0f;
  uint32_t light_seed = 1u;
  float light_min_sep = 0.0f;
  int samples_per_light = 0;
  float r_occ = 15.0f;
  float x_occ = 0.0f;
  float y_from = 40.0f;
  float y_to = -40.0f;
  int frames = 41;
  int samples = 200000;
  float r_pupil = 19.5f;
  float sensor_width = 80.0f;
  int xres = 1024;
  int yres = 1024;
  string out_prefix = "OutputEXR/eclipsed-bokeh/eclipsed";
  bool out_prefix_set = false;
  float light_gain = 500.0f;
  bool write_baseline = false;
  int num_lambdas = 1;
  float lambda_from = 440.0f;
  float lambda_to = 660.0f;
  float lambda_focus_ref = 550.0f;
  bool write_png = false;
  float png_exposure = 1.0f;
  int occluder_ref_mode = 0;
  float occluder_outline_rel = 0.0f;
  float light_depth_label_rel = 0.0f;
  unsigned int seed = 1;
  string case_id;
  const char *lens_file = nullptr;

  int opt;
  while ((opt = getopt(argc, argv, "hi:c:F:E:d:D:L:M:K:N:T:Q:U:V:J:G:R:X:A:B:n:S:P:w:x:y:o:g:O:H:uW:a:b:C:pq:m:s:t:")) != -1) {
    switch (opt) {
      case 'h':
        showUsage(argv[0]);
        return 0;
      case 'i': lens_file = optarg; break;
      case 'c': degree = atoi(optarg); break;
      case 'F': z_focus = atof(optarg); break;
      case 'E': z_focus_end = atof(optarg); z_focus_end_set = true; break;
      case 'd': film_defocus = atof(optarg); break;
      case 'D': film_defocus_end = atof(optarg); film_defocus_end_set = true; break;
      case 'L': z_light = atof(optarg); break;
      case 'M': z_light_end = atof(optarg); z_light_end_set = true; break;
      case 'K': z_occ = atof(optarg); break;
      case 'N': z_occ_end = atof(optarg); z_occ_end_set = true; break;
      case 'T': target_radius = atof(optarg); break;
      case 'Q': light_count = atoi(optarg); break;
      case 'U': light_spread = atof(optarg); break;
      case 'V': light_seed = (uint32_t)strtoul(optarg, nullptr, 10); break;
      case 'J': light_min_sep = atof(optarg); break;
      case 'G': samples_per_light = atoi(optarg); break;
      case 'R': r_occ = atof(optarg); break;
      case 'X': x_occ = atof(optarg); break;
      case 'A': y_from = atof(optarg); break;
      case 'B': y_to = atof(optarg); break;
      case 'n': frames = atoi(optarg); break;
      case 'S': samples = atoi(optarg); break;
      case 'P': r_pupil = atof(optarg); break;
      case 'w': sensor_width = atof(optarg); break;
      case 'x': xres = atoi(optarg); break;
      case 'y': yres = atoi(optarg); break;
      case 'o': out_prefix = optarg; out_prefix_set = true; break;
      case 'g': light_gain = atof(optarg); break;
      case 'u': write_baseline = true; break;
      case 'W': num_lambdas = atoi(optarg); break;
      case 'a': lambda_from = atof(optarg); break;
      case 'b': lambda_to = atof(optarg); break;
      case 'C': lambda_focus_ref = atof(optarg); break;
      case 'p': write_png = true; break;
      case 'q': png_exposure = atof(optarg); break;
      case 'm': occluder_ref_mode = atoi(optarg); break;
      case 'O': occluder_outline_rel = (float)atof(optarg); break;
      case 'H': light_depth_label_rel = (float)atof(optarg); break;
      case 's': seed = (unsigned int)strtoul(optarg, nullptr, 10); break;
      case 't': case_id = optarg; break;
      default:
        showUsage(argv[0]);
        return 1;
    }
  }

  // Optional positional output prefix (handy when you forget -o).
  if (optind < argc && !out_prefix_set) {
    out_prefix = argv[optind++];
    out_prefix_set = true;
  }
  if (optind < argc) {
    cerr << "unexpected extra argument: " << argv[optind] << endl;
    return 1;
  }

  if (!case_id.empty()) {
    if (case_id == "A" || case_id == "a") {
      z_focus = 800.0f;
      z_occ = 1200.0f;
      z_light = 2000.0f;
    } else if (case_id == "B" || case_id == "b") {
      z_occ = 600.0f;
      z_focus = 1000.0f;
      z_light = 2000.0f;
    } else if (case_id == "C" || case_id == "c") {
      z_occ = 600.0f;
      z_light = 1000.0f;
      z_focus = 2500.0f;
    } else {
      cerr << "invalid -t case id: " << case_id << endl;
      return 1;
    }
  }

  if (!z_focus_end_set) z_focus_end = z_focus;
  if (!film_defocus_end_set) film_defocus_end = film_defocus;
  if (!z_light_end_set) z_light_end = z_light;
  if (!z_occ_end_set) z_occ_end = z_occ;

  if (frames < 1 || samples < 1 || degree < 1) {
    cerr << "invalid parameters" << endl;
    return 1;
  }
  if (occluder_ref_mode < 0 || occluder_ref_mode > 3) {
    cerr << "invalid -m (expected 0, 1, 2, or 3)" << endl;
    return 1;
  }
  if (target_radius < 0) {
    cerr << "invalid -T (expected >= 0)" << endl;
    return 1;
  }
  if (light_count < 1 || light_count > 10000) {
    cerr << "invalid -Q (expected 1..10000)" << endl;
    return 1;
  }
  if (!(light_spread > 0.0f)) {
    cerr << "invalid -U (expected > 0)" << endl;
    return 1;
  }
  if (light_min_sep < 0.0f) {
    cerr << "invalid -J (expected >= 0)" << endl;
    return 1;
  }
  if (samples_per_light < 0) {
    cerr << "invalid -G (expected >= 0)" << endl;
    return 1;
  }
  if (num_lambdas < 1 || num_lambdas > 128) {
    cerr << "invalid -W (expected 1..128)" << endl;
    return 1;
  }
  if (lambda_from <= 0 || lambda_to <= 0 || lambda_focus_ref <= 0) {
    cerr << "invalid wavelength (expected > 0)" << endl;
    return 1;
  }
  if (lambda_to < lambda_from) {
    cerr << "invalid wavelength range (expected -b >= -a)" << endl;
    return 1;
  }
  if (!out_prefix.empty() && out_prefix[0] == '-') {
    cerr << "output prefix looks like an option: " << out_prefix << endl;
    cerr << "did you forget the argument to -o? e.g. -o OutputEXR/myrun/frame" << endl;
    return 1;
  }
  if (z_occ <= 0 || z_light <= 0 || z_focus <= 0 || z_occ_end <= 0 || z_light_end <= 0 || z_focus_end <= 0) {
    cerr << "distances must be > 0" << endl;
    return 1;
  }

  ensureOutDirForPrefix(out_prefix);
  srand(seed);

  const float sensor_scaling = xres / sensor_width;

  cout << "focus distance (start/end): " << z_focus << " / " << z_focus_end << " mm" << endl;
  cout << "light distance (start/end): " << z_light << " / " << z_light_end << " mm" << endl;
  cout << "occluder distance (start/end): " << z_occ << " / " << z_occ_end << " mm" << endl;
  cout << "film defocus (start/end): " << film_defocus << " / " << film_defocus_end << " mm" << endl;
  cout << "target radius (-T): " << target_radius << " mm" << endl;
  cout << "lights (-Q): " << light_count << " (spread -U: " << light_spread << " mm, seed -V: " << light_seed
       << ", minSep -J: " << light_min_sep << " mm)" << endl;
  cout << "degree: " << degree << ", samples: " << samples << ", frames: " << frames << endl;
  if (samples_per_light > 0) cout << "samples per light (-G): " << samples_per_light << endl;
  cout << "light gain: " << light_gain << endl;
  cout << "occluder ref mode (-m): " << occluder_ref_mode << endl;
  cout << "wavelengths (-W): " << num_lambdas << " (" << lambda_from << " .. " << lambda_to << " nm)" << endl;
  cout << "focus reference wavelength (-C): " << lambda_focus_ref << " nm" << endl;

  for (int f = 0; f < frames; ++f) {
    const float t = (frames == 1) ? 0.0f : (f / (float)(frames - 1));
    const float y_occ = y_from + (y_to - y_from) * t;

    const float z_focus_t = lerpFloat(z_focus, z_focus_end, t);
    const float z_light_t = lerpFloat(z_light, z_light_end, t);
    const float z_occ_t = lerpFloat(z_occ, z_occ_end, t);
    const float film_defocus_t = lerpFloat(film_defocus, film_defocus_end, t);

    CImg<float> img(xres, yres, 1, 3, 0);
    CImg<float> img_occ;
    if (occluder_ref_mode == 2) img_occ.assign(xres, yres, 1, 3, 0);
    CImg<float> img_open;
    CImg<float> img_diff;

    // Render spectral samples (physically: occluder only blocks; it does not emit light).
    Transform4f lens_focus_ref =
        lens_file ? lens_only_from_file(lens_file, lambda_focus_ref, degree) : lens_only_default(lambda_focus_ref, degree);
    Transform4f system_focus_ref = two_plane_5(z_focus_t, degree) >> lens_focus_ref;
    const float d_film_ref = find_focus_X(system_focus_ref);
    Transform4f to_film = propagate_5(d_film_ref + film_defocus_t, degree);
    Transform4f system_occ_ref = two_plane_5(z_occ_t, degree) >> lens_focus_ref >> to_film;
    Transform4f system_light_ref = two_plane_5(z_light_t, degree) >> lens_focus_ref >> to_film;

    // Deterministic background light layout (same every frame and wavelength).
    vector<Light2D> lights;
    lights.reserve((size_t)light_count);
    uint32_t ls = light_seed;
    const float min_sep2 = light_min_sep * light_min_sep;
    for (int li = 0; li < light_count; ++li) {
      float x = 0.0f;
      float y = 0.0f;
      bool placed = false;
      for (int tries = 0; tries < 2000; ++tries) {
        x = (rand01_u32(ls) - 0.5f) * light_spread;
        y = (rand01_u32(ls) - 0.5f) * light_spread;
        if (light_min_sep <= 0.0f) {
          placed = true;
          break;
        }
        bool ok = true;
        for (const auto &p : lights) {
          const float dx = x - p.x;
          const float dy = y - p.y;
          if (dx * dx + dy * dy < min_sep2) {
            ok = false;
            break;
          }
        }
        if (ok) {
          placed = true;
          break;
        }
      }
      if (!placed) {
        // Fall back: accept the last sample; this keeps determinism but may violate minSep if the area is too dense.
      }
      const float amp = 0.35f + 0.65f * rand01_u32(ls);
      lights.push_back(Light2D{x, y, amp});
    }

    for (int ll = 0; ll < num_lambdas; ++ll) {
      const float lambda_nm =
          (num_lambdas == 1) ? lambda_focus_ref
                             : (lambda_from + (lambda_to - lambda_from) * (ll / (float)(num_lambdas - 1)));

      // Spectral -> RGB weights.
      float rgb[3];
      spectrum_p_to_rgb(lambda_nm, 1.0f, rgb);

      // Build lens at this wavelength.
      Transform4f lens = lens_file ? lens_only_from_file(lens_file, lambda_nm, degree)
                                   : lens_only_default(lambda_nm, degree);

      Transform4f system_light = two_plane_5(z_light_t, degree) >> lens >> to_film;
      Transform4f system_occ = two_plane_5(z_occ_t, degree) >> lens >> to_film;

      const int splat_samples = (samples_per_light > 0) ? samples_per_light : max(1, samples / light_count);
      for (const auto &L : lights) {
        splatBokehPoint(img, system_light, sensor_scaling,
                        L.x, L.y, target_radius, r_pupil, splat_samples,
                        rgb[0] * light_gain * L.amp, rgb[1] * light_gain * L.amp, rgb[2] * light_gain * L.amp,
                        true, z_light_t, z_occ_t, x_occ, y_occ, r_occ);
      }

      if (write_baseline) {
        if (ll == 0) img_open.assign(xres, yres, 1, 3, 0);
        for (const auto &L : lights) {
          splatBokehPoint(img_open, system_light, sensor_scaling,
                          L.x, L.y, target_radius, r_pupil, splat_samples,
                          rgb[0] * light_gain * L.amp, rgb[1] * light_gain * L.amp, rgb[2] * light_gain * L.amp,
                          false, z_light_t, z_occ_t, x_occ, y_occ, r_occ);
        }
      }

      // Visualization only (not a physical “matte” occluder appearance).
      if (occluder_ref_mode == 1 || occluder_ref_mode == 2) {
        if (ll == 0) {
          const int ref_samples = max(1000, samples / 20);
          CImg<float> &target = (occluder_ref_mode == 2) ? img_occ : img;
          splatBokehPoint(target, system_occ, sensor_scaling,
                          x_occ, y_occ, 0.0f, r_pupil, ref_samples,
                          0.12f * light_gain, 0.12f * light_gain, 0.12f * light_gain,
                          false, z_occ_t, z_occ_t, 0, 0, 0);
        }
      }

      if (occluder_ref_mode == 3) {
        if (ll == 0) {
          float in[4] = {x_occ, y_occ, 0.0f, 0.0f};
          float out[4];
          system_occ.evaluate(in, out);
          const int cx = (int)lroundf(out[0] * sensor_scaling + img.width() * 0.5f);
          const int cy = (int)lroundf(out[1] * sensor_scaling + img.height() * 0.5f);
          const float v = 0.10f * light_gain;
          drawCrosshair(img, cx, cy, 8, v, v, v);
        }
      }
    }

    if (write_baseline) subtractClamp(img_open, img, img_diff);
    clampNonNegative(img);
    if (write_baseline) {
      clampNonNegative(img_open);
      clampNonNegative(img_diff);
    }
    if (occluder_ref_mode == 2) clampNonNegative(img_occ);

    float maxv = 0.0f;
    if (occluder_outline_rel > 0.0f || light_depth_label_rel > 0.0f) {
      cimg_forXY(img, x, y) {
        for (int c = 0; c < 3; ++c) maxv = max(maxv, img(x, y, 0, c));
      }
    }

    if (occluder_outline_rel > 0.0f) {
      const float red_value = occluder_outline_rel * maxv;
      drawDashedProjectedOccluderOutline(img, system_occ_ref, sensor_scaling, x_occ, y_occ, r_occ, red_value);
      if (write_baseline) {
        drawDashedProjectedOccluderOutline(img_diff, system_occ_ref, sensor_scaling, x_occ, y_occ, r_occ, red_value);
      }
    }
    if (light_depth_label_rel > 0.0f) {
      const float label_value = light_depth_label_rel * maxv;
      drawLightDepthLabels(img, system_light_ref, sensor_scaling, lights, z_light_t, label_value);
      if (write_baseline) drawLightDepthLabels(img_diff, system_light_ref, sensor_scaling, lights, z_light_t, label_value);
    }

    char exr_path[512];
    snprintf(exr_path, sizeof(exr_path), "%s_%04d.exr", out_prefix.c_str(), f);
    img.save(exr_path);

    if (write_baseline) {
      char open_path[512];
      snprintf(open_path, sizeof(open_path), "%s_open_%04d.exr", out_prefix.c_str(), f);
      img_open.save(open_path);
      char diff_path[512];
      snprintf(diff_path, sizeof(diff_path), "%s_diff_%04d.exr", out_prefix.c_str(), f);
      img_diff.save(diff_path);
    }

    if (occluder_ref_mode == 2) {
      char occ_path[512];
      snprintf(occ_path, sizeof(occ_path), "%s_occ_%04d.exr", out_prefix.c_str(), f);
      img_occ.save(occ_path);
    }

    if (write_png) {
      CImg<unsigned char> png;
      tonemapToSRGB(img, png, png_exposure);
      char png_path[512];
      snprintf(png_path, sizeof(png_path), "%s_%04d.png", out_prefix.c_str(), f);
      png.save(png_path);

      if (write_baseline) {
        CImg<unsigned char> png_open;
        tonemapToSRGB(img_open, png_open, png_exposure);
        char open_png_path[512];
        snprintf(open_png_path, sizeof(open_png_path), "%s_open_%04d.png", out_prefix.c_str(), f);
        png_open.save(open_png_path);

        CImg<unsigned char> png_diff;
        tonemapToSRGB(img_diff, png_diff, png_exposure);
        char diff_png_path[512];
        snprintf(diff_png_path, sizeof(diff_png_path), "%s_diff_%04d.png", out_prefix.c_str(), f);
        png_diff.save(diff_png_path);
      }

      if (occluder_ref_mode == 2) {
        CImg<unsigned char> png_occ;
        tonemapToSRGB(img_occ, png_occ, png_exposure);
        char png_occ_path[512];
        snprintf(png_occ_path, sizeof(png_occ_path), "%s_occ_%04d.png", out_prefix.c_str(), f);
        png_occ.save(png_occ_path);
      }
    }

    cout << "wrote frame " << f << " (y_occ=" << y_occ << ", zL=" << z_light_t << ", zK=" << z_occ_t
         << ", zF=" << z_focus_t << ", d=" << film_defocus_t << ")" << endl;
  }

  return 0;
}
