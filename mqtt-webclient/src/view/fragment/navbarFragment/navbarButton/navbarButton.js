import "./navbarButton.css";
import { Link, NavLink } from "react-router-dom";

export default function navbarButton({ text, link, icon }) {
  return (
    <NavLink to={link} className={`navbarButton`} href={link}>
      {text}
    </NavLink>
  );
}
