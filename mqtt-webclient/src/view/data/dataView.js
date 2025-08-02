import "./dataView.css";
import "../views.css";
import Navbar from "../fragment/navbarFragment/navbarFragment";

export default function dataView() {
  return (
    <div className={"mainContainer"}>
      {/*<Navbar linkIsClicked={linkIsClicked} />*/}
      <h1>Data</h1>
      <Navbar />
    </div>
  );
}
