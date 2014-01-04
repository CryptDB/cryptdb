package client;

import java.io.PrintStream;

public class SimpleSystemPrinter implements SimplePrinter {
  PrintStream out;

  public SimpleSystemPrinter(PrintStream out) {
    this.out = out;
  }

  public void println(String msg) {
    if (out != null)
      out.println(msg);
  }
}
