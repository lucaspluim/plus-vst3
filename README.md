# + (Plus) VST3

An audio visualizer plugin with customizable effects that respond to frequency bands.

## Features

- **Real-time Audio Visualization**: Responds to incoming audio from your DAW track
- **Multiple Effect Types**:
  - Flutter: Gradual color fade based on frequency energy
  - Binary Flash: On/off flash effect with threshold detection
  - Starfield: 3D particle effect that reacts to audio
  - Frequency Line: Waveform display of selected frequency ranges
- **Customizable Frequency Ranges**: Map effects to specific frequency bands (Sub-Bass, Bass, Mids, Highs, Kick Transient, etc.)
- **Light/Dark Mode**: Toggle between light and dark backgrounds
- **Color Customization**: Choose custom colors for each effect
- **Drag & Drop Interface**: Easily assign effects to different screen sections

## Installation

### Pre-built Binary (macOS)

1. Download `+.vst3` from the `Release/` folder
2. Copy it to `~/Library/Audio/Plug-Ins/VST3/`
3. Restart your DAW or rescan plugins
4. Load "+" as an effect on any audio track

### Building from Source

**Requirements:**
- macOS 15.3+ (Apple Silicon)
- Xcode with command line tools
- CMake 3.22+
- JUCE framework (expected at `../JUCE` relative to project root)

**Build Steps:**

```bash
# Clone the repository
git clone https://github.com/lucaspluim/plus-vst3.git
cd plus-vst3

# Ensure JUCE is available at ../JUCE
# If not, clone JUCE: git clone https://github.com/juce-framework/JUCE.git ../JUCE

# Generate Xcode project
cmake -B Builds/MacOSX -G Xcode

# Build the plugin
cd Builds/MacOSX
xcodebuild -configuration Release

# The plugin will be automatically installed to ~/Library/Audio/Plug-Ins/VST3/
```

## Usage

1. Load the "+" plugin on any audio track in your DAW
2. Audio will automatically flow through the plugin and trigger visualizations
3. **Double-click** anywhere to open the effect picker
4. **Drag effects** from the picker onto different screen sections (top, bottom-left, bottom-right)
5. **Right-click** a section to change its frequency range
6. Use the **light mode toggle** to switch between dark and light backgrounds
7. **Color picker** lets you customize effect colors before dragging

## Keyboard Shortcuts

- **O**: Open audio file (Standalone version only)
- **Space**: Play/Pause (Standalone version only)
- **D**: Toggle debug frequency values
- **L**: Toggle light/dark mode

## Technical Details

- **Plugin Format**: VST3, AU, Standalone
- **Audio Processing**: Pass-through (audio in = audio out)
- **Analysis**: Real-time FFT with 2048 sample window
- **Frequency Bands**: 9 ranges from Sub-Bass (20-60Hz) to Very Highs (8000-20000Hz)
- **Refresh Rate**: 60 FPS

## Architecture

- Built with JUCE framework
- Separate rendering for VST3/AU (track audio) and Standalone (file loading)
- Runtime wrapper type detection for format-specific behavior
- Independent starfield instances per screen section
- Adaptive normalization for consistent visualization across different audio levels

## License

All rights reserved.

## Credits

Developed with assistance from Claude (Anthropic).
