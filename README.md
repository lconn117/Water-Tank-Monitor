# Water Tank Monitor

Remote water tank level monitoring using a solar-powered 4G sensor and a PWA dashboard.

## Architecture

```
┌─────────────────────────────┐        ┌──────────────────┐
│  LILYGO T-SIM7080G-S3       │        │  Firebase        │
│  + Pressure transducer      │─HTTPS─▶│  Firestore       │
│  + 18650 + solar panel      │        └────────┬─────────┘
│  SIM: 1NCE (LTE-M)          │                 │ realtime
└─────────────────────────────┘                 ▼
                                       ┌──────────────────┐
                                       │  PWA (React)     │
                                       │  Firebase Hosting│
                                       │  "Add to Home    │
                                       │   Screen" on iOS │
                                       └──────────────────┘
```

## Repository layout

```
├── firmware/          # PlatformIO / Arduino C++ (device code)
│   ├── src/main.cpp
│   ├── include/config.h
│   └── platformio.ini
├── app/               # React PWA (user-facing dashboard)
│   ├── src/
│   ├── index.html
│   └── package.json
├── .github/workflows/ # CI: auto-deploy app on push to main
├── firebase.json      # Firebase Hosting config
└── .firebaserc        # Firebase project reference
```

## Getting started

### 1. Firebase project setup

1. Create a project at [console.firebase.google.com](https://console.firebase.google.com)
2. Enable **Firestore** and **Authentication → Email/Password**
3. Create a web app and copy the config into `app/src/firebase.js`
4. Replace `your-firebase-project-id` in `firmware/include/config.h` and `.firebaserc`

### 2. Firestore data structure

```
users/{uid}
  devices: ["tank-01", "tank-02"]   ← array of device IDs

devices/{deviceId}
  readings/{auto-id}
    levelPercent: number
    timestamp: timestamp
```

Security rules: users can only read devices listed in their own user document.

### 3. Firmware (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
2. Open the `firmware/` folder in VS Code
3. Edit `include/config.h` — set `FIREBASE_PROJECT_ID`, `DEVICE_ID`, and sensor calibration
4. Build & upload via PlatformIO toolbar

### 4. Web app (local dev)

```bash
cd app
npm install
npm run dev        # http://localhost:5173
```

### 5. Deploy app

```bash
# One-off: install Firebase CLI
npm install -g firebase-tools
firebase login

# Deploy manually
cd app && npm run deploy

# Or: push to main branch → GitHub Actions deploys automatically
```

### 6. CI/CD setup (GitHub Actions auto-deploy)

1. In Firebase console → Project settings → Service accounts → Generate new private key
2. Add the JSON as a GitHub secret named `FIREBASE_SERVICE_ACCOUNT`
3. Add a GitHub variable named `FIREBASE_PROJECT_ID` with your project ID
4. Every push to `main` that touches `app/` will auto-build and deploy

## Hardware BOM

| Item | Source | Approx cost |
|---|---|---|
| LILYGO T-SIM7080G-S3 | AliExpress | ~$30 |
| 1NCE SIM card | 1nce.com | ~$15 lifetime |
| Submersible pressure transducer (0–5m, IP68) | AliExpress / DFRobot | $15–35 |
| 18650 LiPo cell (3000mAh) | AliExpress | ~$5 |
| 5W solar panel | AliExpress | ~$8 |
| 165Ω resistor (4-20mA → ADC) | Any | <$1 |
| Weatherproof enclosure | AliExpress | ~$8 |

**Ongoing:** ~$0 (Firebase free tier, 1NCE lifetime SIM)
