# Hosting (GitHub Pages)

This repository deploys the MkDocs site using **GitHub Pages + GitHub Actions**.

## 1. Enable GitHub Pages (one-time)

1. Open the repository on GitHub.
2. Go to `Settings → Pages`.
3. Under **Build and deployment**, set **Source** to **GitHub Actions**.

## 2. Deploy

1. Push to `main`.
2. Go to `Actions` tab and open the latest workflow run:
   - **Deploy MkDocs to GitHub Pages**
3. Once it finishes, the Pages URL will be shown in the `deploy` job (environment: `github-pages`).

Typical URL format:

- `https://<owner>.github.io/<repo>/`

## Workflow file

- `.github/workflows/pages.yml`

