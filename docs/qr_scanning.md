---
attachments: [Clipboard_2025-12-01-16-07-20.png]
title: QR code scanning
created: '2025-11-28T04:49:31.338Z'
modified: '2025-12-01T08:23:47.894Z'
---

# QR code scanning
## working principle
After connecting the camera with the Raspberry Pi, take a screenshot by calling rpicam still. Scan the captured image with a QR code, and if there is a QR code in the image, print out the QR code information.
## Environment Configuration
### Create a virtual environment
![](@attachment/Clipboard_2025-12-01-16-07-20.png)
sudo apt update
sudo apt install -y python3-venv python3-pip
cd ros2_ws/src/qr_scanner/scripts/
python3 -m venv qr_venv
### Install dependencies
sudo apt install -y libzbar0 libzbar-dev python3-pip python3-pil python3-numpy
source ros2_ws/src/qr_scanner/scripts/qr_venv/bin/activate
pip3 install pyzbar pillow
## execute program
source ros2_ws/src/qr_scanner/scripts/qr_venv/bin/activate
sudo python3 test_camera.py
