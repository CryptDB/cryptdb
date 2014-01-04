/*
 * JOutputArea - Simple output area for jTPCC
 *
 * Copyright (C) 2003, Raul Barbosa 
 * Copyright (C) 2004-2006, Denis Lussier
 *
 */

import java.awt.*;
import javax.swing.*;

public class JOutputArea extends JScrollPane
{
    public final static long DEFAULT_MAX_CHARS = 20000, NO_CHAR_LIMIT = 0;

    private JTextArea jTextArea;
    private long counter;
    private long maxChars;

    public JOutputArea(String text)
    {
        super();

        setVerticalScrollBarPolicy(VERTICAL_SCROLLBAR_ALWAYS);
        setHorizontalScrollBarPolicy(HORIZONTAL_SCROLLBAR_AS_NEEDED);
        jTextArea = new JTextArea(text);
        jTextArea.setEditable(false);
        jTextArea.setFont(new Font("Courier New", Font.BOLD, 12));
        maxChars = DEFAULT_MAX_CHARS;
        getViewport().add(jTextArea);
    }

    public JOutputArea()
    {
        this("");
    }

    public void print(String text)
    {
        if(getText().length() > maxChars) this.clear();

        jTextArea.append(text);
        jTextArea.setCaretPosition(jTextArea.getText().length());
    }

    public void println(String text)
    {
        print(text+"\n");
    }

    public void clear()
    {
        jTextArea.setText("");
        jTextArea.setCaretPosition(0);
    }

    public String getText()
    {
        return jTextArea.getText();
    }

    public void setMaxChars(long maxChars)
    {
        this.maxChars = maxChars;
    }
}