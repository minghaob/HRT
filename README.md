# HRT
HRT is a tools to track visited locations in a 100% run of BotW. It works by monitoring the streamed game frames from a camera / video and detecting if the frames contain location names.

## Requirements
1. Currently runs only on Windows.
2. OBS version 29+, if you want to run HRT while streaming / capturing using OBS.
3. Only English in-game language is supported at the moment.

> Support of other OSs and in-game languages can be added if there's demand.

## How to Use
Check [here](https://www.youtube.com/watch?v=sGRodAIGf-c) for a video demo.

### Installation
Download the release package and extract its contents to an arbitrary folder.

### Setting up OBS Virtual Camera
> Required if you want to run HRT while streaming / capturing with OBS. It's because Windows doesn't allow multiple programs to access the same camera simultaneously. HRT wouldn't be able to access the capture device while OBS is reading from it.
1. Launch OBS.
2. In the "Controls" panel (activated from Main Menu -> File -> Docks -> Controls), open virtual camera settings by clicking the cogwheel button next to "Start Virtual Camera". (Upgrade to OBS 29+ if the button is missing from the panel)
3. Under "Output Type" select "Source", and under "Output Selection" select your source for the capture device. Click OK to save the settings.
4. In the "Controls" panel, click "Start Virtual Camera". OBS will now keep forwarding the source input to its virtual camera.

### Run HRT
1. Launch HRT.exe.
2. Choose the input camera. Select "OBS Virtual Camera" if you've set it up as above. Otherwise select the camera of your capture device.
3. Wait until the line "**Run "webui.bat" to start the web-ui**" shows up in console.
4. Run **webui.bat** to start the web-ui.

### Web UI
1. Click "View Input Image" button to show the current input frame, make sure it matches the game screen. (If it displays the whole OBS scene instead of just the game frame, you need to change OBS virtual camera settings as described in section "Setting up OBS Virtual Camera")
2. Leave web-ui open in background and proceed with the run as usual.
3. When Link visits a location, the corresponding cell in the table would turn green and a log message would be added.
> Note: Many unmissable locations, typically shrines, towers and divine beasts, are not included in the table to keep it compact. These locations will still show up in the log though.

> Note: Click a location name in the table to view it on [objmap](https://objmap.zeldamods.org/).
4. When taking breaks during a run, it is safe to terminate HRT and close the web-ui, the progress is automatically saved in browser's local storage. When resuming a run, start HRT and web-ui again.
5. Click "Clear" button when resetting or starting a new run.

> IMPORTANT: Before using HRT in a real run, visit a location casually and check if it's detected properly. It might be possible that sometimes specially filters are applied to the input image and it becomes too bright / dark for HRT to work.

## Known Issues
Recorded videos of the following runs were used for testing:
* [BingsF 15:32](https://www.speedrun.com/botw/run/y6ode1py)
* [Coensi 15:52](https://www.speedrun.com/botw/run/zgp40xvz)
* [DJCarmichael 19:20](https://www.speedrun.com/botw/run/zgv9p6vz)
* [minghao 16:15](https://www.speedrun.com/botw/run/yol0dx5y)
* [Paperario 18:28](https://www.speedrun.com/botw/run/yj5r8w7m)
* [SrTapir 17:34](https://www.speedrun.com/botw/run/y6ovw9jy)
> Unfortunately many of the other runs on the leaderboard have some overlay covering the location text of the game frame and can't be used for testing.

No major issues were found during the tests. The only minor issue was that "Gerudo Town" might be detected as "Gerudo Tower" in some rare cases. Since both are unmissable locations, it is left as a known issue.

## How to Build
1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/).
2. Install [vcpkg](https://github.com/microsoft/vcpkg).
3. Open command line in vcpkg folder and run the following commands to build OpenCV and Tesseract, and integrate vcpkg into Visual Studio.
```
vcpkg install opencv4[world]:x64-windows-static
vcpkg install tesseract:x64-windows-static
vcpkg integrate install
```
4. Download the latest ffmpeg build [here](https://github.com/BtbN/FFmpeg-Builds/releases). (Get **ffmpeg-master-latest-win64-lgpl-shared.zip**, you might need to click "Show all 50 assets" to find it.) Extract its contents to the root folder of this repository.
5. Open "HRT.sln" and build. Find the artifacts in the "bin" folder.

## Acknowledgements
HRT uses the following libraries:

### Tesseract
https://github.com/tesseract-ocr/tesseract

### OpenCV
https://github.com/opencv/opencv

### FFmpeg
https://github.com/FFmpeg/FFmpeg

### Simple-Web-Server and Simple-WebSocket-Server
https://gitlab.com/eidheim/Simple-Web-Server

https://gitlab.com/eidheim/Simple-WebSocket-Server
