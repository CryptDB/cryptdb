#pragma once

/*
 * The rob template below is sufficient to trick gcc to access private
 * or protected members, bypassing the expected access checking rules.
 * However, the code below isn't strictly allowed by the C++ spec;
 * e.g., clang rejects it.
 *
 * It is possible to access private or protected members in a way that
 * is well-specified and legal, by exploiting the following rule from
 * C++ specification section 14.7.2, "Explicit instantiation":
 *
 *   http://www.lcdf.org/c++/clause14.html#s14.7.2
 *
 *   The usual access checking rules do not apply to names used to
 *   specify explicit instantiations.
 *
 * A standard-compliant (and more complex) version of the rob template,
 * which exploits the above rule, is available here:
 *
 *   http://bloglitb.blogspot.com/2010/07/access-to-private-members-thats-easy.html
 */

template<typename Victim, typename FieldType, FieldType Victim::*p>
struct rob {
  static FieldType Victim::*ptr() { return p; }
};

