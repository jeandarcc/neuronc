// C#-style declarations demo (single-class-per-file friendly)
Status enum { Pending, Running, Done };

CSharpFeatures interface {
  Draw method() as void;
}

Init method() as int {
  color is Color.Red as Color;
  value is 10 as int;

  value as maybe dynamic then string then float;

  switch (color) {
    case Color.Red:
      Print("Red");
      break;
    case Color.Yellow:
      Print("Yellow");
      break;
    case Color.Green:
      Print("Green");
      break;
    default:
      Print("Unknown");
      break;
  }


  return 0;
}

Color enum {
  Red,
  Yellow,
  Green
}
