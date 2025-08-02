import "./graphsView.css";
import "../views.css";
import Navbar from "../fragment/navbarFragment/navbarFragment";

export default function graphsView() {
  return (
    <div className={"mainContainer"}>
      {/*<Navbar linkIsClicked={linkIsClicked} />*/}
      <h1>Graphs</h1>
      <Navbar />
    </div>
  );
}
