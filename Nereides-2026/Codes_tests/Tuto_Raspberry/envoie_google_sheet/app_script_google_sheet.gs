function doPost(e) {
  try {
    var sheet = SpreadsheetApp.openById("11b5vX6QZdGRNoCQUotirvyUhAKqi1KHWM3dZBYk80gI").getSheetByName("Reception");  //ID du sheet, à trouver dans son URL ;-)
    var data = JSON.parse(e.postData.contents);

    // --- Aplatir le JSON ---
    var flatData = {};
    for (var sensor in data) {
      for (var key in data[sensor]) {
        flatData[sensor + "_" + key] = data[sensor][key];
      }
    }

    // --- Lire l'entête actuelle ---
    var lastCol = sheet.getLastColumn();
    var headers = lastCol > 0 ? sheet.getRange(1, 1, 1, lastCol).getValues()[0] : [];

    // --- Si entête vide, créer Timestamp + colonnes actuelles ---
    if (!headers || headers.length === 0 || headers[0] === "") {
      headers = ["Timestamp"];
      for (var col in flatData) headers.push(col);
      sheet.appendRow(headers);
      lastCol = sheet.getLastColumn(); // mettre à jour le vrai nombre de colonnes
    }

    // --- Ajouter de nouvelles colonnes si elles n'existent pas ---
    for (var col in flatData) {
      if (headers.indexOf(col) === -1) {
        headers.push(col);
        lastCol = sheet.getLastColumn();
        sheet.insertColumnAfter(lastCol);       // ajouter une colonne vide après la dernière
        sheet.getRange(1, lastCol + 1).setValue(col); // nom de la nouvelle colonne
      }
    }

    // --- Construire la ligne de données ---
    var row = [new Date()];
    for (var i = 1; i < headers.length; i++) {
      row.push(flatData[headers[i]] !== undefined ? flatData[headers[i]] : "");
    }

    // --- Ajouter la ligne ---
    sheet.appendRow(row);

    return ContentService
      .createTextOutput(JSON.stringify({status: "ok"}))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (err) {
    return ContentService
      .createTextOutput(JSON.stringify({status: "error", message: err.toString()}))
      .setMimeType(ContentService.MimeType.JSON);
  }
}
