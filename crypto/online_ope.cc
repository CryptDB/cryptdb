#include <string.h>
#include <string>
#include <crypto/online_ope.hh>
#include <iostream>
#include <cmath>

#include <sstream>


template<class EncT>
struct tree_node {
    EncT enc_val;
    tree_node *left;
    tree_node *right;

    tree_node(const EncT &ev) : enc_val(ev), left(0), right(0) {}
    ~tree_node() {
        if (left)
            delete left;
        if (right)
            delete right;
    }

    uint64_t height() {
        uint64_t h = 0;
        if (left)
            h = max(h, 1 + left->height());
        if (right)
            h = max(h, 1 + right->height());
        return h;
    }

    uint64_t count() {
        uint64_t n = 1;
        if (left)
            n += left->count();
        if (right)
            n += right->count();
        return n;
    }
};

template<class EncT>
string print(tree_node<EncT> * n) {
    if (!n) {
	return "NULL";
    } else {
	stringstream ss;
	ss << n->enc_val;
	return ss.str();
    }
}

template<class EncT>
void print_tree(tree_node<EncT> * n) {
    if (n) {
	cerr << "node " << n->enc_val << " has left " << print(n->left) << " and right " << print(n->right) << "\n";
	print_tree(n->left);
	print_tree(n->right);
    }
}

template<class EncT>
tree_node<EncT> *
ope_server<EncT>::tree_lookup(tree_node<EncT> *root, uint64_t v, uint64_t nbits) const
{
    if (nbits == 0) {
	return root;
    }

    if (!root) {
        return 0;
    }

    return tree_lookup((v&(1ULL<<(nbits-1))) ? root->right : root->left, v, nbits-1);
}

/////////////////////////////////////////////////////

/*
 * Rivest and Galperin's tree balancing: log n memory
 */

template<class EncT>
static tree_node<EncT> *
flatten(tree_node<EncT> * x, tree_node<EncT> * y) {
    //cerr << "flatten x " << print(x) << " y " << print(y) << "\n";
    if (!x) {
	return y;
    }
    x->right = flatten(x->right, y);
    return flatten(x->left, x);
}

template<class EncT>
static tree_node<EncT> *
build_tree(uint64_t n, tree_node<EncT> * x) {
    // std::cerr << "build tree n " << n << " x " << print(x) << "\n";
    if (n == 0) {
	x->left = NULL;
	return x;
    }
    tree_node<EncT> * r = build_tree(ceil(1.0 * (n-1)/2.0), x);
    tree_node<EncT> * s = build_tree(floor(1.0 * (n-1)/2.0), r->right);

    r->right = s->left;
    s->left = r;

    return s;
}

template<class EncT>
void
ope_server<EncT>::relabel(tree_node<EncT> * parent, bool isLeft, uint64_t size) {

    tree_node<EncT> * scapegoat;
    if (parent == NULL) {
	scapegoat = root;
    } else {
	scapegoat = (isLeft == 1) ? parent->left : parent->right;
    }

    tree_node<EncT> * w = new tree_node<EncT>(0);
    tree_node<EncT> * z = flatten(scapegoat, w);

    build_tree(size, z);

    if (parent) {
	if (isLeft) {
	    parent->left = w->left;
	} else {
	    parent->right = w->left;
	}
    } else {
	//root has changed
	root = w->left;
    }

    w->left = 0;    /* Something seems fishy here */
    delete w;
}

////////////////////////////////////////////////////


template<class EncT>
void
ope_server<EncT>::tree_insert(tree_node<EncT> **np, uint64_t v,
			      const EncT &encval, uint64_t nbits, uint64_t pathlen)
{
    if (nbits == 0) {
        throw_c(*np == 0);

        tree_node<EncT> *n = new tree_node<EncT>(encval);
        *np = n;
	update_tree_stats(pathlen);
	if (trigger(pathlen)) {
	    bool isLeft;
	    uint64_t subtree_size;
	    tree_node<EncT> * parent = node_to_balance(v, pathlen, isLeft, subtree_size);
     	    relabel(parent, isLeft, subtree_size);
	} else {

	}

    } else {
        throw_c(*np);
        tree_insert((v&(1ULL<<(nbits-1))) ? &(*np)->right : &(*np)->left,
                    v, encval, nbits-1, pathlen);
    }
}

static bool
isWeightUnbalanced(uint64_t left, uint64_t right, double alpha) {

    uint64_t total = left + 1 + right;

    if ((left > total * alpha) || (right > total * alpha)) {
	return true;
    }

    return false;
}

template<class EncT>
static tree_node<EncT> *
unbalanced_node(tree_node<EncT> * parent, tree_node<EncT> * current,
		const uint64_t v, const uint64_t nbits,const double alpha,
		bool & isLeft, uint64_t & child_size, bool & found, uint64_t & total_size) {

    if (nbits == 0) {//found v node
	child_size = 1;
	return current;
    }

    if (!current)  {//v does not exist
	return NULL;
    }

    //next child to go to in search for v
    tree_node<EncT> * child = (v&(1ULL<<(nbits-1))) ? current->right : current->left;

    tree_node<EncT> * res = unbalanced_node(current, child,
					    v, nbits-1, alpha,
					    isLeft, child_size, found, total_size);
    if (res == NULL) {
	//search failed
	return res;
    }
    if (found) {//the unbalanced node has been found
	return res;
    }
    //unbalanced node not yet found: test if current node is unbalanced
    uint64_t other_child_size = 0;

    if (child == current->right) {
	other_child_size = (current->left == NULL) ? 0 : current->left->count();
    } else {
	other_child_size = (current->right == NULL) ? 0 : current->right->count();
    }

    if (isWeightUnbalanced(child_size, other_child_size, alpha)) {
	found = true;
	total_size = child_size + other_child_size + 1;
	if (parent != NULL) {
	    isLeft = (parent->left == current);
	}
	return parent;
    } else {
	//this node is balanced, recursion takes us up
        child_size = child_size + 1 + other_child_size;
	return res;
    }

}


template<class EncT>
tree_node<EncT> *
ope_server<EncT>::node_to_balance(uint64_t v, uint64_t nbits,  bool & isLeft, uint64_t & subtree_size)
{
    bool found = false;
    uint64_t child_size = 0;

    tree_node<EncT> * unb = unbalanced_node((tree_node<EncT> *)0, root,
					    v, nbits, scapegoat_alpha,
					    isLeft, child_size, found, subtree_size);

    throw_c(found); //math guarantees an unbalanced node on v's path to root exists

    return unb;
}


template<class EncT>
void
ope_server<EncT>::update_tree_stats(uint64_t path_len)
{
    num_nodes++;
}


template<class EncT>
bool
ope_server<EncT>::trigger(uint64_t path_len) const
{
    //basic scapegoat trigger
    if ((path_len > 1) && (path_len > log(num_nodes)/log(1/scapegoat_alpha) + 1)) {
	return true;
    } else {
	return false;
    }

}

template<class EncT>
EncT
ope_server<EncT>::lookup(uint64_t v, uint64_t nbits) const
{
    auto n = tree_lookup(root, v, nbits);
    if (!n) {
        throw ope_lookup_failure();
    }

    return n->enc_val;

}

template<class EncT>
void
ope_server<EncT>::insert(uint64_t v, uint64_t nbits, const EncT &encval)
{
    tree_insert(&root, v, encval, nbits, nbits);

}

template<class EncT>
ope_server<EncT>::ope_server()
{
    root = NULL;
    max_height = 0;
    num_nodes = 0;
}

template<class EncT>
ope_server<EncT>::~ope_server()
{
    if (root)
        delete root;
}

/*
 * Explicitly instantiate the ope_server template for various ciphertext types.
 */
template class ope_server<uint64_t>;
template class ope_server<uint32_t>;
template class ope_server<uint16_t>;


