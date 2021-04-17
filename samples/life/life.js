
displaylist = [];

function paint() {
    cv = document.getElementById('cv');
    cx = cv.getContext("2d");
    cx.fillStyle = 'black';
    displaylist.forEach((op) => op(cx));
    displaylist = [];
}

const url = "ws://" + window.location.hostname + ":8002/life";

ws = new WebSocket(url);

ws.onmessage = function(event) {
  //console.log(event.data);
  Function(event.data)();
};

document.body.addEventListener("click", (event) => {
  if (event.target.tagName == "BUTTON") {
    switch (event.target.value) {
      case 'glider':
        //ws.send('........$...@....$....@...$..@@@...$........$!');
        ws.send('........$');
        ws.send('...@....$');
        ws.send('....@...$');
        ws.send('..@@@...$');
        ws.send('........$');
        ws.send('!');
        break;
      case 'step':
        ws.send('!');
        break;
    }
  }
});

