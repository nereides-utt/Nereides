import "./navbarFragment.css";
import NavbarButton from "./navbarButton/navbarButton";
//import { Link } from "react-router-dom";
export default function navbarFragment() {
  const navbarItems = [
    {
      text: "Realtime",
      link: "/",
      icon: "https://i.imgur.com/1bX5QH6.jpg",
    },
    { text: "Graphs", link: "/Graphs", icon: "realtime_icon.png" },
    { text: "Data", link: "/Data", icon: "realtime_icon.png" },
    { text: "Settings", link: "/Settings", icon: "realtime_icon.png" },
  ];
  return (
    <div className={"navbar"}>
      {navbarItems.map((item) => (
        <NavbarButton
          text={item.text}
          link={item.link}
          isActive={item.isActive}
        />
      ))}
    </div>
  );
}
