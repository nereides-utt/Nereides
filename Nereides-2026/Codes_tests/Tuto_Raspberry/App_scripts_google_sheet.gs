function onEdit(e) { //Qd on change qqch
  if (!e) return;
  Logger.log("Modification détectée : " + e.range.getA1Notation() + " = " + e.value);
  const sheet = e.range.getSheet();
  if (sheet.getName() !== "Graphiques") return;
  
  const colCheckbox = 2; // colonne B
  const rowStart = 6; // ligne de départ
  if (e.range.getColumn() === colCheckbox && e.range.getRow() >= rowStart) {
    const checked = e.value === "TRUE"; // case cochée ?
    const name = sheet.getRange(e.range.getRow(), 1).getValue(); // nom de variable en colonne A
    if (checked) {
      createGraph(name);
    } else {
      removeGraph(name);
    }
  }
}

function createGraph(name) { //Ajouter le graph sélectionné
  const ss = SpreadsheetApp.getActive();
  const sheet = ss.getSheetByName("Graphiques");
  const dataSheet = ss.getSheetByName("Valeurs_a_afficher");

  const existing = sheet.getCharts()
    .filter(c => c.getOptions().get("title") === name);
  if (existing.length > 0) return;

  const headers = dataSheet
    .getRange(1, 1, 1, dataSheet.getLastColumn())
    .getValues()[0];

  const colIndex = headers.indexOf(name);
  if (colIndex === -1) return;

  const lastRow = dataSheet.getLastRow();

  const timestamps = dataSheet.getRange("A2:A");
  const values     = dataSheet.getRange(2, colIndex + 1, dataSheet.getMaxRows(), 1);

  // 🔍 calcul min / max des valeurs
  const valuesData = values.getValues()
    .flat()
    .filter(v => typeof v === "number");

  if (valuesData.length === 0) return;

  const min = Math.min(...valuesData);
  const max = Math.max(...valuesData);

  // petite marge (5%)
  const padding = (max - min) * 0.05 || 1;

  const chartRow = 1+ sheet.getCharts().length * 18;

  const chart = sheet.newChart()
    .setChartType(Charts.ChartType.LINE)
    .addRange(timestamps)
    .addRange(values)
    .setPosition(chartRow, 4, 0, 0)
    .setOption("title", name)
    .setOption("hAxis.title", "Timestamp")
    .setOption("hAxis.format", "yyyy-MM-dd HH:mm:ss")
    .setOption("vAxis.viewWindow", {
      min: min - padding,
      max: max + padding
    })
    .build();

  sheet.insertChart(chart);
}

function removeGraph(name) { //Retirer le graph décoché
  const sheet = SpreadsheetApp.getActive().getSheetByName("Graphiques");
  const charts = sheet.getCharts();
  charts.forEach(c => {
    if (c.getOptions().get("title") === name) {
      sheet.removeChart(c);
    }
  });
}



function doPost(e) { //Qd on recoit une valeur
  try {
    var sheet = SpreadsheetApp.openById("11b5vX6QZdGRNoCQUotirvyUhAKqi1KHWM3dZBYk80gI").getSheetByName("Reception"); //ID du sheet, c'est la chaîne de caractères dans l'URL ;-)
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
