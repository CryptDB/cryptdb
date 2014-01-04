package edu.mit.benchmark.wikipedia;

public class Article{

	public String userText;
	public int pageId;
	public String oldText;
	public int textId;
	public int revisionId;

	
	public Article(String userText, int pageId, String oldText, int textId,	int revisionId) {
		super();
		this.userText = userText;
		this.pageId = pageId;
		this.oldText = oldText;
		this.textId = textId;
		this.revisionId = revisionId;
	}
	
}