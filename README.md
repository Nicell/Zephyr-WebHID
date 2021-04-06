# Running this Project:
1. `west build -b nrf52840dk_nrf52840` inside `zephyr-app`
2. `west flash with nRF52840 DK` plugged into the usb on the narrow side (J-Link side)
3. After flash plug in nRF52840 DK on wide side (actual nRF52840 usb)
4. `npm i -g serve` to install the serve utility
5. Move to the `web` directory and run `serve`
6. Open up the generated link, open devtools, click the connect button, select and open the `WebHID Example`
7. Now on press of "Button 1" of the nRF52840 DK, you should get console logs describing what's happening and the LEDs should update on the dk