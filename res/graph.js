/*

  Graph support functions

  This code all belongs in `defjs` functions, but I haven't
  written the compiler for that yet.

*/

paintchart = function () {
    cv = document.getElementById('cv');
    ctx = cv.getContext("2d");
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, width, height);
    ctx.save();
    ctx.scale(width/13, height/3);
    ctx.clearRect(0, 0, 13, 3);
    ctx.translate(0, 1.5);
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(13, 0);
    ctx.moveTo(0, 0);
    displaylist(ctx);
    ctx.restore();
    ctx.stroke();
}

menu = function (msg) {
    webSocket.send(msg.target.id);
}

