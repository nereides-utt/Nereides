import "../rectangles.css";
import "./rectangleGauge.css";

export default function rectangleGauge({
  title,
  unit,
  value,
  maxValue,
  invertMax,
}) {
  let percentage = (value / maxValue) * 100;
  if (percentage < 0) {
    percentage = 0;
  } else if (percentage > 100) {
    percentage = 100;
  }
  const maxThreshold = 0.66 * maxValue;
  const minThreshold = 0.33 * maxValue;

  let color = "green";
  if (percentage < minThreshold) {
    color = invertMax ? "red" : "green";
  } else if (percentage >= minThreshold && percentage <= maxThreshold) {
    color = "orange";
  } else {
    color = invertMax ? "green" : "red";
  }
  return (
    <div className="rectangle">
      <div className="rectangleTitle">
        <span>{title}</span>
      </div>
      <div className="rectangleGaugeValue">
        {value}
        {unit}
      </div>
      <div className="rectangleGaugeWrapper">
        <div
          className="rectangleGaugeProgress"
          style={{
            width: `${percentage}%`,
            backgroundColor: `${color}`,
          }}
        ></div>
      </div>
    </div>
  );
}
