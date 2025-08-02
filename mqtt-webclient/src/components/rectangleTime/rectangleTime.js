import "../rectangles.css";
import "./rectangleTime.css";

export default function rectangleTime({ title, time }) {
  let hours = Math.floor(time / 3600);
  let rem = time % 3600;
  let minutes = Math.floor(rem / 60);
  let seconds = rem % 60;
  return (
    <div className="rectangle rectangleTime">
      <div className="rectangleTimeTitle">
        <span>{title}</span>
      </div>
      <div className="rectangleTimeValue">
        {hours}h {minutes}m {seconds}s
      </div>
    </div>
  );
}
