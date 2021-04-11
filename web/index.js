let deviceFilter = { vendorId: 0x2fe3, productId: 0x100 }; // Default Zephyr VID/PID
let requestParams = { filters: [deviceFilter] };

let device;

function handleConnectedDevice(e) {
  console.log("Device connected: " + e.device.productName);
  e.device.open().then(() => {
    e.device.addEventListener("inputreport", handleInputReport);
    console.log("Re-opened device and re-attached inputreport listener");
    device = e.device;
    readValue();
  });
}

function handleDisconnectedDevice(e) {
  console.log("Device disconnected: " + e.device.productName);
}

function handleInputReport(e) {
  console.log(e.device.productName + ": got input report " + e.reportId);
  const input = new Uint8Array(e.data.buffer);
  console.log(new Uint8Array(e.data.buffer));

  const range = document.querySelector("#range");

  range.value = input[0];
}

function handleRangeUpdate(value) {  
  let report = new Uint8Array([1, value]);
  device.sendReport(0x01, report).then(() => {
    console.log("Sent output report " + report);
  });
}

function saveValue() {
  const range = document.querySelector("#range");

  let report = new Uint8Array([2, range.value]);
  device.sendReport(0x01, report).then(() => {
    console.log("Sent output report " + report);
  });
}

function readValue() {
  let report = new Uint8Array([3]);
  device.sendReport(0x01, report).then(() => {
    console.log("Sent output report " + report);
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
      device = devices[0];
      readValue();
    });
  });
}
