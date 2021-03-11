/*

  Graph support functions

  This code all belongs in `defjs` functions, but I haven't
  written the compiler for that yet.

*/

displaylist = [];

paintchart = function () {
    cv = document.getElementById('cv');
    ctx = cv.getContext("2d");
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, cv.width, cv.height);
    ctx.save();
    ctx.scale(cv.width/13, cv.height/3);
    ctx.clearRect(0, 0, 13, 3);
    ctx.translate(0, 1.5);
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(13, 0);
    ctx.moveTo(0, 0);
    displaylist.forEach((op) => op(ctx));
    ctx.restore();
    ctx.stroke();
}

menu = function (msg) {
    webSocket.send(msg.target.id);
}

