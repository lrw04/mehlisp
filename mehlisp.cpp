#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <string>
#include <vector>
using ld = long double;
using ll = long long;

#define ERR_EXIT(...)                 \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        exit(EXIT_FAILURE);           \
    } while (false)

struct trie {
    struct node {
        char path;
        ll depth;
        node *ch[128], *father;
        node(char c = 0) {
            path = c;
            for (int i = 0; i < 128; i++) ch[i] = nullptr;
            father = nullptr;
        }
    };
    node *root;
    trie() {
        root = new node();
        root->depth = 0;
    }
    node *intern(const char *s, int n) {
        node *u = root;
        for (int i = 0; i < n; i++) {
            if (!u->ch[(int)s[i]]) u->ch[(int)s[i]] = new node(s[i]);
            u->ch[(int)s[i]]->depth = u->depth + 1;
            u->ch[(int)s[i]]->father = u;
            u = u->ch[(int)s[i]];
        }
        return u;
    }
    void write_string(node *u, char *s) {
        if (u->father) {
            write_string(u->father, s);
            s[u->depth - 1] = u->path;
        }
    }
};

trie obarray;

enum type {
    number,
    symbol,
    cons,
    lambda,
    macro,
    env,
    primitive,
};

struct tp;

using primitive_t = tp(tp, tp);

// Tagged Pointer
struct tp {
    type t;
    union {
        ld n;
        trie::node *s;
        ll p;
        primitive_t *fp;
    };
};

tp intern(const char *s, int len) {
    tp p;
    p.t = symbol;
    p.s = obarray.intern(s, len);
    return p;
}
tp intern(const char *s) { return intern(s, strlen(s)); }

tp eq(tp a, tp b) {
    if (a.t != b.t) return intern("nil");
    if (a.t >= cons) return a.p == b.p ? intern("t") : intern("nil");
    if (a.t == number) return a.n == b.n ? intern("t") : intern("nil");
    // symbol
    return a.s == b.s ? intern("t") : intern("nil");
}
bool to_boolean(tp a) { return a.t != symbol || a.s != intern("nil").s; }

std::vector<tp> cars, cdrs;
std::list<ll> freel, allocl, rootl;
std::vector<bool> mark;

ll memory_size;
const ll initial_memory = 4096;

void gc_init() {
    cars.resize(initial_memory);
    cdrs.resize(initial_memory);
    mark.resize(initial_memory);
    memory_size = initial_memory;
    for (ll i = 0; i < initial_memory; i++) freel.push_back(i);
}

void gc_mark(ll p) {
    if (p < 0 || mark[p]) return;
    mark[p] = true;
    if (cars[p].t >= cons) gc_mark(cars[p].p);
    if (cdrs[p].t >= cons) gc_mark(cdrs[p].p);
}

void gc_collect() {
    for (auto p : allocl) mark[p] = false;
    for (auto p : rootl) gc_mark(p);
    for (auto it = allocl.begin(); it != allocl.end();) {
        if (!mark[*it]) {
            freel.push_front(*it);
            it = allocl.erase(it);
        } else {
            it++;
        }
    }
}

void gc_expand() {
    cars.resize(memory_size * 2);
    cdrs.resize(memory_size * 2);
    mark.resize(memory_size * 2);
    for (ll i = memory_size; i < memory_size * 2; i++) freel.push_back(i);
    memory_size *= 2;
}

ll gc_alloc() {
    if (freel.empty()) gc_collect();
    if (freel.empty()) gc_expand();
    if (freel.size()) {
        auto p = freel.front();
        freel.pop_front();
        return p;
    } else {
        ERR_EXIT("Out of memory");
    }
}

tp make_number(ld n) {
    tp ret;
    ret.t = number;
    ret.n = n;
    return ret;
}

tp make_cons(tp car, tp cdr, type t = cons) {
    ll p = gc_alloc();
    cars[p] = car;
    cdrs[p] = cdr;
    tp ret;
    ret.t = t;
    ret.p = p;
    return ret;
}
tp cons_car(tp c) {
    if (c.t != cons) ERR_EXIT("cons_car on non-cons");
    return cars[c.p];
}
tp cons_cdr(tp c) {
    if (c.t != cons) ERR_EXIT("cons_cdr on non-cons");
    return cdrs[c.p];
}

tp make_lambda(tp vars, tp body, tp env) {
    return make_cons(vars, make_cons(body, env), lambda);
}
tp lambda_vars(tp l) {
    if (l.t != lambda) ERR_EXIT("lambda_vars on non-lambda");
    return cars[l.p];
}
tp lambda_body(tp l) {
    if (l.t != lambda) ERR_EXIT("lambda_body on non-lambda");
    return cars[cdrs[l.p].p];
}
tp lambda_env(tp l) {
    if (l.t != lambda) ERR_EXIT("lambda_env on non-lambda");
    return cdrs[cdrs[l.p].p];
}

tp make_macro(tp vars, tp body, tp env) {
    return make_cons(vars, make_cons(body, env), macro);
}
tp macro_vars(tp m) {
    if (m.t != macro) ERR_EXIT("macro_vars on non-macro");
    return cars[m.p];
}
tp macro_body(tp m) {
    if (m.t != macro) ERR_EXIT("macro_body on non-macro");
    return cars[cdrs[m.p].p];
}
tp macro_env(tp m) {
    if (m.t != macro) ERR_EXIT("macro_env on non-macro");
    return cdrs[cdrs[m.p].p];
}

tp make_env(tp bindings, tp father) { return make_cons(bindings, father, env); }
tp env_bindings(tp e) {
    if (e.t != env) ERR_EXIT("env_bindings on non-env");
    return cars[e.p];
}
tp env_father(tp e) {
    if (e.t != env) ERR_EXIT("env_father on non-env");
    return cdrs[e.p];
}
tp env_lookup(tp e, tp s) {
    if (!to_boolean(e)) ERR_EXIT("Unbound variable");
    for (auto cur = env_bindings(e); to_boolean(e); cur = cons_cdr(cur)) {
        auto p = cons_car(cur);
        if (to_boolean(eq(cons_car(p), s))) return cur;
    }
    return env_lookup(env_father(e), s);
}
tp env_extend(tp e, tp p) {
    return make_env(make_cons(p, env_bindings(e)), env_father(e));
}

tp env_init() {}

tp eval(tp expr, tp e) {}

bool number_str_p(std::string s) {
    ld _;
    try {
        _ = stold(s);
    } catch (const std::invalid_argument &e) {
        return false;
    }
    return _ * _ >= -0.5;  // Need to find a way to use the value of _
}

tp read_cdr(std::istream &st);

std::string read_token(std::istream &st) {
    auto c = st.get();
    while (isspace(c) && c != EOF) c = st.get();
    if (c == EOF) return "";
    std::string s;
    s += c;
    if (c == '(' || c == ')') return s;
    while ((!isspace(c = st.get())) && c != EOF && c != '(' && c != ')') s += c;
    if (c == '(' || c == ')') st.unget();
    return s;
}

tp read(std::istream &st) {}

tp read_cdr(std::istream &st) {}

void print(tp expr, std::ostream &st) {}

int main() {
    gc_init();
    auto env = env_init();
    while (true) {
        std::cout << "? " << std::flush;
        auto expr = read(std::cin);
        if (to_boolean(eq(expr, intern("EOF")))) break;
        auto res = eval(expr, env);
        print(res, std::cout);
        std::cout << std::endl;
    }
}
