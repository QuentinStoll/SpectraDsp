---
title:          Beta Test Plan  
subtitle:       SpectraDsp (Audio Plugin & Standalone)  
author:         Quentin STOLL, Laurent ZHANG, Lucas RYCHLEWSKI, Sebastien PICHOT  
module:         G-EIP-700  
version:        1.0  
---

## **1. Project context**
**SpectraDsp** is an open-source guitar amplifier plugin, shipped as the standalone application and plugin **Spectra** (VST3/AU). Developed using the **JUCE framework**, it plays neural amplifier models and cabinet impulse responses, saves tones as profiles, and can clone a cabinet or an amplifier from the standalone app. The project focuses on high-fidelity playback, low latency, and a structure other people can build on. It includes multi-platform CI/CD (Windows, macOS, Linux), real-time performance metrics, and technical documentation.

## **2. User Roles**
The following roles are involved in the beta testing phase.

| **Role Name** | **Description** |
| --- | --- |
| **End User** | Musicians or producers using the plugin for real-time audio processing without perceptible delay. |
| **Contributor** | External developers seeking to understand the architecture, submit bug fixes, or improve the DSP engine. |
| **QA Engineer** | Internal team members responsible for validating metrics (CPU/Latency), unit tests, and CI/CD pipeline health. |

---

## **3. Feature table**
All features listed below will be demonstrated during the beta defense.

| **Feature ID** | **User role** | **Feature name** | **Short description** |
| --- | --- | --- | --- |
| **F1** | End User | Create Digital Amp Copy | Analyzes your real amplifier and creates a digital version that sounds just like it |
| **F2** | End User | Instant Response (No Delay) | Ensures you hear sound immediately when you play no lag or delay |
| **F3** | End User | Save Amplifier Sound | Saves your amplifier's sound as a file you can use again or share with others |
| **F4** | End User | Load Amplifier Sound | Opens a saved amplifier sound file to use that specific tone |
| **F5** | End User | Save Complete Session | Saves your entire current setup (which amp, all settings, knob positions) as a preset |
| **F6** | End User | Load Complete Session | Opens a saved preset to restore all your previous settings exactly as they were |
| **F7** | End User | Save Personal Preferences | Remembers your favorite settings (colors, performance options, etc.) between sessions |
| **F8** | End User | Load Personal Preferences | Automatically applies your saved preferences when you open the plugin |
| **F9** | End User | Control Interface | Visual controls (knobs and switches) to adjust your amplifier settings |
| **F10** | End User | Audio Level Display | Shows real-time visual meters so you can see your audio levels |
| **F11** | End User / Contributor | Simple Installation | Easy install process for using the plugin or contributing to development |
| **F12** | Contributor | Technical Guide | Detailed documentation explaining how the technology works |
| **F13** | Contributor | Code Comments | Clear explanations written directly in the code to help developers understand it |
| **F14** | Contributor | Open Source Ease of Contribution | Public access to all code with guidelines for contributing improvements (README, license, contribution guidelines) |
| **F15** | QA Engineer | Automated Build System | Automatic testing that checks if the software works on Windows, Mac, and Linux |
| **F16** | QA Engineer | Tests and Coverage | Automatic unit tests for funtion of the project and automatic coverage |
| **F17** | QA Engineer | Performance Data | Files that track how well the software performs for analysis |
| **F18** | QA Engineer | Error Tracking System | Captures technical details and errors to help fix problems |
| **F19** | End User / QA Engineer | Optional Error Reporting | Lets users choose to share their hardware details in logs to help improve the software |
| **F20** | QA Engineer | Error Alert Messages | Pop-up warnings that appear when something goes wrong |

| **F21** | QA Engineer | Automated Quality Checks | Automatic tests that verify the audio processing works correctly |
---

## **4. Success criteria**
| **Feature ID** | **Key success criteria** | **Indicator/metric** | **Result** |
| --- | --- | --- | --- |
| **F1** | Sound Accuracy | Digital copy sounds almost identical to the real amplifier | Pending |
| **F2** | Instant Response | Sound delay is less than ~10 milliseconds (imperceptible to humans) | Achieved |
| **F3** | Save Amplifier Sound | File is created successfully with all sound characteristics included | Pending |
| **F4** | Load Amplifier Sound | Loaded sound matches the saved version with 100% accuracy | Pending |
| **F5** | Save Complete Session | File is created successfully with all the states for elements of the plugin included | Pending |
| **F6** | Load Complete Session | Opens a saved state of the plugin to restore it to how it was before last closing it | Pending |
| **F7** | Save Personal Preferences | File is created successfully with all the settings (colors, performance options, etc.) included | Pending |
| **F8** | Load Personal Preferences | Automatically applies your saved settings when you open the plugin | Pending |
| **F9** | Control Interface Smoothness | Interface runs smoothly at 60 frames per second without affecting audio | Achieved |
| **F10** | Audio Level Display | Visible UI element showing real-time visual meters of the audio levels | Pending |
| **F11** | Simple Installation | A new developer can install and build the project in under 15 minutes | Achieved |
| **F12/F13** | Code Quality Standards | All code meets formatting standards and passes quality checks | Pending |
| **F14** | Open Source | The project repository is open source | Achieved |
| **F14** | Ease of Contribution | The project repository and/or its wiki contains a README LICENSE and CONTRIBUTING files | Achieved |
| **F15** | Automated Build System | Software builds successfully on Windows, Mac, and Linux at the same time | Achieved |
| **F16** | Tests and Coverage | The projects CI/CD performs unit tests for funtion of the project and automatic coverage | Pending |
| **F17** | Performance Data | Performance data file are generated at runtime if the setting is enabled | Pending |
| **F18** | Error Tracking System | Errors warnings and other debug messages are generated and stored in a log file | Achieved |
| **F19** | Optional Error Reporting | Existance of an opt in setting in the plugin to enable identifiable info in logs | Pending |
| **F20** | Error Alert Messages | Toggleable and configurable popups for logs to apears in plugin's UI | Pending |
| **F21** | Automated Quality Checks | Presence of CI/CD element to check sound accuracy | Pending |