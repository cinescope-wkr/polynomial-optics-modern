# Hosting (GitHub Pages)

This repository publishes its documentation site via **GitHub Pages** and builds/deploys it via **GitHub Actions**.

- Live site: `https://cinescope-wkr.github.io/polynomial-optics-modern/`
- Deploy workflow: `.github/workflows/pages.yml`
- MkDocs config: `docs/mkdocs.yml`
- Docs dependencies: `docs/requirements-docs.txt`

**Maintainer**: Jinwoo Lee (`cinescope@kaist.ac.kr`)

## Enable GitHub Pages (one-time)

1. Open the repository on GitHub.
2. Go to `Settings → Pages`.
3. Under **Build and deployment**, set **Source** to **GitHub Actions**.

## Deploy

1. Push to `main`.
2. Go to `Actions` tab and open the latest workflow run:
   - **Deploy MkDocs to GitHub Pages**
3. Once it finishes, the Pages URL will be shown in the `deploy` job (environment: `github-pages`).

Typical URL format:

- `https://<owner>.github.io/<repo>/`

## Local preview

From repo root:

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r docs/requirements-docs.txt
mkdocs serve -f docs/mkdocs.yml
```

## Troubleshooting

- If Pages is enabled but the site is stale, check the latest Actions run for `Deploy MkDocs to GitHub Pages`.
- If the workflow fails at `mkdocs build --strict`, fix broken links/anchors in `docs/` and re-push.
