package edu.mit.benchmark.wikipedia;

/** Immutable class containing information about transactions. */
public final class Transaction {
	public final boolean isUpdate;
	public final int userId;
	public final int nameSpace;
	public final String pageTitle;

	public Transaction(boolean isUpdate, int userId, int nameSpace,
			String pageTitle) {
		this.isUpdate = isUpdate;
		// value of -1 indicate user is not logged in
		this.userId = userId;
		this.nameSpace = nameSpace;
		this.pageTitle = pageTitle;
	}
}
