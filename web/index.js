let deviceFilter = { vendorId: 0x2fe3, productId: 0x100 }; // Default Zephyr VID/PID
let requestParams = { filters: [deviceFilter] };

function handleConnectedDevice(e) {
  console.log("Device connected: " + e.device.productName);
  e.device.open().then(() => {
    e.device.addEventListener("inputreport", handleInputReport);
    console.log("Re-opened device and re-attached inputreport listener");
  });
}

function handleDisconnectedDevice(e) {
  console.log("Device disconnected: " + e.device.productName);
}

function handleInputReport(e) {
  console.log(e.device.productName + ": got input report " + e.reportId);
  console.log(new TextDecoder().decode(new Uint8Array(e.data.buffer)));

  let report = new Uint8Array([Math.ceil(Math.random() * 15)]);
  console.log("Sending report " + report[0]);
  e.device.sendReport(0x01, report).then(() => {
    console.log("Sent output report " + report[0]);
  });
}

function hidTest() {

  navigator.hid.addEventListener("connect", handleConnectedDevice);
  navigator.hid.addEventListener("disconnect", handleDisconnectedDevice);

  navigator.hid.requestDevice(requestParams).then((devices) => {
    if (devices.length == 0) return;
    devices[0].open().then(() => {
      console.log("Opened device: " + devices[0].productName);

      devices[0].addEventListener("inputreport", handleInputReport);
    });
  });
}
