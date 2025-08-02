import "./settingsView.css";
import "../views.css";
import Navbar from "../fragment/navbarFragment/navbarFragment";
export default function settingsView() {
  return (
    <div className={"mainContainer"}>
      {/*<Navbar linkIsClicked={linkIsClicked} />*/}
      <h1>Settings</h1>
      <Navbar />
    </div>
  );
}
