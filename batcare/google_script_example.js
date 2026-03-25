function doPost(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  var params = e.parameter;
  sheet.appendRow([
    params.fecha,
    params.hora,
    params.estado,
    params.voltaje,
    params.valadc,
    params.activacion
  ]);
  return ContentService.createTextOutput("OK");
}

