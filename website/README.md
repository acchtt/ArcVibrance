# ArcVibrance Website

Responsive, dependency-free landing page for ArcVibrance, published at
[acchtt.github.io/ArcVibrance](https://acchtt.github.io/ArcVibrance/).

## Run locally

From this directory:

```bash
python -m http.server 8080
```

Then open `http://localhost:8080`.

## Files

- `index.html` — page structure and content
- `styles.css` — responsive styling
- `script.js` — mobile navigation and small header behaviors
- `assets/arcvibrance-logo.png` — official ArcVibrance logo
- `assets/arcvibrance-app.png` — application screenshot

Pushes that change this folder are deployed by
`.github/workflows/deploy-pages.yml`.
