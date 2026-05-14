# Landlink

build firmware

```
pio --version
cd firmware && pio run -e ttgo-t-beam-sx1262

pio run -e ttgo-t-beam-sx1262 -t upload
pio device monitor -e ttgo-t-beam-sx1262
```

build ios app

```
npm run build && npx cap sync ios
```
