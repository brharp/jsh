svgns = "http://www.w3.org/2000/svg";
svg = document.createElementNS(svgns, "svg");
circle = document.createElementNS(svgns, "circle");
circle.setAttributeNS(null, "cx", 25);
circle.setAttributeNS(null, "cy", 25);
circle.setAttributeNS(null, "r",  20);
circle.setAttributeNS(null, "fill", "green"); 
svg.appendChild(circle);
document.body.appendChild(svg);
