import NavbarFragment from "../../view/fragment/navbarFragment/navbarFragment";
export default function navbarController() {
  function linkIsClicked() {
    console.log("Link has been clicked !");
  }
  return <NavbarFragment />;
}
