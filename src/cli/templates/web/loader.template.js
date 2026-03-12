(function() {
  var canvas = document.getElementById('{{CANVAS_ID}}');
  if (!canvas) {
    canvas = document.createElement('canvas');
    canvas.id = '{{CANVAS_ID}}';
    document.body.appendChild(canvas);
  }
  window.Module = window.Module || {};
  window.Module.canvas = canvas;
})();