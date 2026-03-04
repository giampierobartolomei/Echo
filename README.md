# Echo – An AI‑based Toy for Symbolic Play Recognition and Data Collection 

![Echo Prototype](IMG_6805.HEIC)

---

## Overview
**Echo** is an intelligent toy designed to recognize and log **symbolic play activities** in children with Autism Spectrum Conditions.  
It combines onboard sensors, real‑time classification, and a custom Android application to support both **therapy** and **data collection** within the IM‑TWIN research framework.

Echo detects and classifies manipulative play gestures using embedded machine‑learning models trained on inertial data. The system provides **sound and light feedback**, while also logging structured data for offline analysis.

---

## Hardware Setup
- **Microcontroller:** ESP32 (with integrated MPU6050 IMU)  
- **Sensors:** 3‑axis accelerometer (±2 g) and gyroscope (±500 °/s), tilt angles 0–360°  
- **Storage:** microSD for local data logging  
- **Outputs:** LEDs and speaker for multimodal feedback  
- **Prototype files:** `.fzz` breadboard, schematic, and PCB layouts are included in the repo.  
- **Audio:** `play_sounds/` folder includes example `.mp3` files for feedback behavior.

---

## Firmware and Algorithms

### Folders and Functions
- **GetAngle** – Arduino code for raw data acquisition and tilt computation.  
- **getangle_classification** – firmware for **real‑time gesture classification** and control of LEDs/sounds.  
- **first_classify** – experimental early version for serial‑only inference (without majority voting).  
- **fft_computation** and **dt_model** – signal processing and decision‑tree model training, integrated into `getangle_classification`.

### Signal and Range
- Accelerometer: ±2 g (gravity compensation active)  
- Gyroscope: ±500 °/s  
- Tilt angles: 0–360°

### Model Training
The **Decision Tree (DT)** classifier is trained on ten labeled recordings per subject.  
Training and testing can be reproduced via Google Colab notebooks provided in:
```
python/spectrumclassification/
```
Originally the models were exported manually, later automatically translated to C (`model.h`) using **microMLgen**.

---

## Android App – `MPU_TOY_V0`

The `MPU_TOY_V0` folder contains a **Godot** project that serves as the companion **Android app** for Echo.

### Building and Exporting
1. Open the project in **Godot**.  
2. Select *Android Export* and verify the Android build template.  
3. If prompted to rebuild `android/build`, copy the original `AndroidManifest.xml` to the new directory.  
   This ensures the app can save log files containing sensor readings and labels.

For installation issues, refer to the **IM‑TWIN‑App** repository.

### App Features
- Real‑time plotting of **tilt** and **accelerometer** data.  
- **Label selection buttons** for organizing recorded gestures.  
- Automatic creation of labeled log files.  
- Volume and LED sliders (v1.0.1 update): LEDs turn on at startup, and users can control brightness and sound volume.  
- Bluetooth connection with Echo for real‑time classification feedback.

---

## Python and Colab Tools

Offline analysis and model generation are handled via Python and Colab scripts in:
```
python/spectrumclassification/
```
These scripts cover:
- Spectral feature extraction (FFT‑based).  
- Decision‑tree training and export.  
- Automated conversion to embedded C headers using **microMLgen**.  
- Example notebooks for model validation and data visualization.

---

## Data Workflow

1. Run **Echo** with `getangle_classification` firmware.  
2. Connect the Android **MPU_TOY_V0 App**.  
3. Select gesture label and record data (logged to SD and app).  
4. Export `.csv` files for training using the Colab scripts.  
5. Generate a new `model.h` and flash it to the device for updated recognition.  

---

## Repository Layout
```
Echo/
├─ GetAngle/                     # raw acquisition firmware
├─ getangle_classification/      # final classification firmware
├─ first_classify/               # experimental test firmware
├─ MPU_TOY_V0/                   # Godot Android app project
├─ python/spectrumclassification # Colab + Python tools
├─ fft_computation/              # preprocessing scripts
├─ dt_model/                     # DT training artifacts
├─ play_sounds/                  # audio feedback
├─ data_analysis/                # test data and model generation
└─ README.md
```

---

## Citations

**1. Giampiero Bartolomei, Beste Ozcan, Giovanni Granato, Gianluca Baldassarre, and Valerio Sperati. 2025. Echo: an AI-based toy to encourage symbolic play in children with Autism Spectrum Conditions. In Proceedings of the Nineteenth International Conference on Tangible, Embedded, and Embodied Interaction (TEI '25). Association for Computing Machinery, New York, NY, USA, Article 85, 1–6. https://doi.org/10.1145/3689050.3705987**  

**2. Bartolomei, G., Ozcan, B., Granato, G., Baldassarre, G., & Sperati, V. (2025). A proposal for an AI-based toy to encourage and assess symbolic play in autistic children. Behaviour & Information Technology, 44(14), 3390–3403. https://doi.org/10.1080/0144929X.2025.2523478**



---

## License
Code and documentation released under the **MIT License**.  
Audio and media assets under **CC BY 4.0**.

