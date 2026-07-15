// TeX-lite mathtext engine — a subset port of matplotlib._mathtext.
// Box model (Hlist/Vlist/Char/Kern/Rule), layout constants, and the
// subsuper/_genfrac/sqrt algorithms follow mpl 3.10.8; glyph positions are
// near-mpl, not glyph-exact (PARITY D19). Symbol names come from the
// generated tex2uni table (tools/gen_tables.py).
#include "text/mathtext.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>
#include <string_view>
#include <optional>
#include <utility>

#include "graphlib/errors.hpp"
#include "text/font_manager.hpp"

namespace graphlib::detail {

namespace {

#include "mathtext_tables.inc"

// mpl FontConstantsBase (DejaVu Sans uses the base values unchanged).
constexpr double kScriptSpace = 0.05; // x script_space
constexpr double kSubdrop = 0.4;
constexpr double kSup1 = 0.7;
constexpr double kSub1 = 0.3;
constexpr double kSub2 = 0.5;
constexpr double kDelta = 0.025;
constexpr double kDeltaSlanted = 0.2;
constexpr double kDeltaIntegral = 0.1;
constexpr double kShrinkFactor = 0.7; // mpl SHRINK_FACTOR
// mpl TruetypeFonts.get_underline_thickness: hardcoded (0.75/12)*fontsize*dpi/72.
constexpr double kRuleFactor = 0.0625; // x em

struct SpaceCommand {
    std::string_view name;
    double em_fraction;
};
// Parser._space_widths.
constexpr std::array<SpaceCommand, 12> kSpaceCommands = {{{",", 0.16667},
                                                          {"thinspace", 0.16667},
                                                          {"/", 0.16667},
                                                          {">", 0.22222},
                                                          {":", 0.22222},
                                                          {";", 0.27778},
                                                          {" ", 0.33333},
                                                          {"enspace", 0.5},
                                                          {"quad", 1.0},
                                                          {"qquad", 2.0},
                                                          {"!", -0.16667},
                                                          {"~", 0.33333}}};

// Parser._overunder_symbols / _dropsub_symbols, as codepoints.
constexpr std::array<char32_t, 12> kOverunder = {0x2211, 0x220F, 0x2210, 0x22C0, 0x22C1, 0x22C2,
                                                 0x22C3, 0x2A00, 0x2A01, 0x2A02, 0x2A04, 0x2A06};
constexpr std::array<char32_t, 7> kDropsub = {0x222B, 0x222C, 0x222D, 0x2A0C,
                                              0x222E, 0x222F, 0x2230};

template <size_t N> bool contains_cp(const std::array<char32_t, N>& arr, char32_t cp) {
    return std::find(arr.begin(), arr.end(), cp) != arr.end();
}

bool in_sorted(std::span<const char32_t> arr, char32_t cp) {
    return std::binary_search(arr.begin(), arr.end(), cp);
}

char32_t lookup_symbol(std::string_view name) {
    const auto* begin = std::begin(kTex2Uni);
    const auto* end = std::end(kTex2Uni);
    const auto* it = std::lower_bound(begin, end, name,
                                      [](const TexSymbol& s, std::string_view n) {
                                          return s.name < n;
                                      });
    if (it != end && it->name == name) {
        return it->codepoint;
    }
    throw ValueError("mathtext: unknown symbol: \\" + std::string(name));
}

// ------------------------------------------------------------------ boxes

constexpr int kNumSizeLevels = 6; // mpl NUM_SIZE_LEVELS

struct Node {
    double width = 0.0;
    double height = 0.0; // above baseline
    double depth = 0.0;  // below baseline
    double shift = 0.0;  // vertical shift (down) applied by the parent Hlist
    int size_level = 0;

    virtual ~Node() = default;
    // mpl List.shrink scales shift_amount; our shift lives on every node
    // (the stretched radical is a bare Char, mpl's AutoHeightChar is a List).
    virtual void shrink() {
        ++size_level;
        if (size_level < kNumSizeLevels) {
            shift *= kShrinkFactor;
        }
    }
    /// Render with the reference point (baseline-left) at (x, y), y-up.
    virtual void render(Path& /*out*/, double /*x*/, double /*y*/) const {}
    [[nodiscard]] virtual bool is_char() const { return false; }
};
using NodeP = std::unique_ptr<Node>;

struct Kern final : Node {
    explicit Kern(double w) { width = w; }
    void shrink() override {
        Node::shrink();
        if (size_level < kNumSizeLevels) {
            width *= kShrinkFactor;
        }
    }
};

struct Box final : Node { // empty box with fixed dimensions (Hbox/Vbox)
    Box(double w, double h, double d) {
        width = w;
        height = h;
        depth = d;
    }
    void shrink() override {
        Node::shrink();
        if (size_level < kNumSizeLevels) {
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
    }
};

struct Rule final : Node { // filled rectangle (fraction bar, sqrt overline)
    Rule(double w, double h, double d) {
        width = w;
        height = h;
        depth = d;
    }
    void shrink() override {
        Node::shrink();
        if (size_level < kNumSizeLevels) {
            width *= kShrinkFactor;
            height *= kShrinkFactor;
            depth *= kShrinkFactor;
        }
    }
    void render(Path& out, double x, double y) const override {
        out.move_to(x, y - depth);
        out.line_to(x + width, y - depth);
        out.line_to(x + width, y + height);
        out.line_to(x, y + height);
        out.close_subpath();
    }
};

struct Char final : Node {
    char32_t cp;
    double em;
    FaceId face;
    bool slanted;
    double advance = 0.0;
    double y_scale = 1.0; // sqrt radical stretch (AutoHeightChar-lite)

    Char(char32_t c, double em_px, FaceId f, bool slant) : cp(c), em(em_px), face(f),
                                                           slanted(slant) {
        update_metrics();
    }
    void update_metrics() {
        const auto& fm = FontManager::instance();
        GlyphMetrics m = fm.glyph_metrics(cp, em, face);
        if (!m.valid && face == FaceId::oblique) {
            face = FaceId::regular; // math symbols live in the upright face
            m = fm.glyph_metrics(cp, em, face);
        }
        if (!m.valid) {
            throw ValueError("mathtext: font has no glyph for U+" +
                             std::to_string(static_cast<unsigned long>(cp)));
        }
        advance = m.advance;
        width = m.advance; // advance-based packing (D19; mpl uses ink width + kern)
        height = std::max(0.0, m.ymax) * y_scale;
        depth = std::max(0.0, -m.ymin) * y_scale;
    }
    void shrink() override {
        Node::shrink();
        if (size_level < kNumSizeLevels) {
            em *= kShrinkFactor;
            update_metrics();
        }
    }
    void render(Path& out, double x, double y) const override {
        const auto& fm = FontManager::instance();
        if (y_scale == 1.0) {
            fm.append_glyph(out, cp, x, y, em, face);
            return;
        }
        Path glyph;
        fm.append_glyph(glyph, cp, 0.0, 0.0, em, face);
        Path scaled = glyph.transformed(Affine2D::scaling(1.0, y_scale));
        out.append(scaled, x, y);
    }
    [[nodiscard]] bool is_char() const override { return true; }
};

struct List : Node {
    std::vector<NodeP> children;
};

struct Hlist final : List {
    explicit Hlist(std::vector<NodeP> kids, bool do_kern = true) {
        children = std::move(kids);
        if (do_kern) {
            kern_pass();
        }
        hpack();
    }
    void kern_pass() {
        std::vector<NodeP> out;
        out.reserve(children.size() * 2);
        for (size_t i = 0; i < children.size(); ++i) {
            const bool has_next = i + 1 < children.size();
            const Char* c1 = dynamic_cast<const Char*>(children[i].get());
            const Char* c2 = has_next ? dynamic_cast<const Char*>(children[i + 1].get())
                                      : nullptr;
            double k = 0.0;
            if (c1 != nullptr && c2 != nullptr && c1->face == c2->face && c1->em == c2->em) {
                k = FontManager::instance().kern(c1->cp, c2->cp, c1->em, c1->face);
            }
            out.push_back(std::move(children[i]));
            if (k != 0.0) {
                out.push_back(std::make_unique<Kern>(k));
            }
        }
        children = std::move(out);
    }
    void hpack() {
        width = 0.0;
        height = 0.0;
        depth = 0.0;
        for (const NodeP& c : children) {
            width += c->width;
            height = std::max(height, c->height - c->shift);
            depth = std::max(depth, c->depth + c->shift);
        }
    }
    /// Center the content in a target width (mpl HCentered + hpack 'exactly').
    void center_to(double target) {
        const double pad = (target - width) / 2.0;
        std::vector<NodeP> out;
        out.reserve(children.size() + 2);
        out.push_back(std::make_unique<Kern>(pad));
        for (NodeP& c : children) {
            out.push_back(std::move(c));
        }
        out.push_back(std::make_unique<Kern>(pad));
        children = std::move(out);
        hpack();
        width = target;
    }
    void shrink() override {
        Node::shrink();
        for (NodeP& c : children) {
            c->shrink();
        }
        if (size_level <= kNumSizeLevels) {
            hpack();
        }
    }
    void render(Path& out, double x, double y) const override {
        double pen = x;
        for (const NodeP& c : children) {
            c->render(out, pen, y - c->shift);
            pen += c->width;
        }
    }
};

struct Vlist final : List {
    // Reference point: the baseline of the LAST child (TeX vpack).
    explicit Vlist(std::vector<NodeP> kids) {
        children = std::move(kids);
        vpack();
    }
    void vpack() {
        width = 0.0;
        double total = 0.0;
        for (const NodeP& c : children) {
            if (dynamic_cast<const Kern*>(c.get()) != nullptr) {
                total += c->width; // a kern's width acts vertically in a Vlist
                continue;
            }
            width = std::max(width, c->width);
            total += c->height + c->depth;
        }
        if (!children.empty()) {
            depth = children.back()->depth;
        }
        height = total - depth;
    }
    void shrink() override {
        Node::shrink();
        for (NodeP& c : children) {
            c->shrink();
        }
        if (size_level <= kNumSizeLevels) {
            vpack();
        }
    }
    void render(Path& out, double x, double y) const override {
        double top = y + height;
        for (const NodeP& c : children) {
            const Kern* k = dynamic_cast<const Kern*>(c.get());
            if (k != nullptr) { // kern acts vertically inside a Vlist
                top -= k->width;
                continue;
            }
            c->render(out, x, top - c->height);
            top -= c->height + c->depth;
        }
    }
};

// ------------------------------------------------------------------ parser

struct Atom {
    NodeP node;
    char32_t cp = 0; // nonzero when the atom is (or wraps) a single symbol
    bool slanted = false;
};

class MathParser {
public:
    MathParser(std::u32string_view src, double em) : src_(src), em_(em) {}

    NodeP parse() {
        std::vector<NodeP> nodes = parse_list(/*inside_group=*/false);
        return std::make_unique<Hlist>(std::move(nodes));
    }

private:
    std::u32string_view src_;
    size_t pos_ = 0;
    double em_;
    int script_depth_ = 0; // inside sub/superscript: no operator spacing
    char32_t prev_symbol_ = 0;

    [[nodiscard]] bool eof() const { return pos_ >= src_.size(); }
    [[nodiscard]] char32_t peek() const { return eof() ? 0 : src_[pos_]; }
    char32_t next() { return src_[pos_++]; }
    void skip_spaces() {
        while (!eof() && peek() == U' ') {
            ++pos_;
        }
    }

    [[nodiscard]] double rule_thickness() const { return kRuleFactor * em_; }
    [[nodiscard]] double em_advance() const { // italic 'm' advance (mpl _make_space)
        return FontManager::instance().glyph_metrics(U'm', em_, FaceId::oblique).advance;
    }

    std::vector<NodeP> parse_list(bool inside_group) {
        std::vector<NodeP> nodes;
        for (;;) {
            skip_spaces();
            if (eof()) {
                if (inside_group) {
                    throw ValueError("mathtext: expected '}'");
                }
                break;
            }
            if (peek() == U'}') {
                if (!inside_group) {
                    throw ValueError("mathtext: unexpected '}'");
                }
                ++pos_;
                break;
            }
            nodes.push_back(parse_item());
        }
        return nodes;
    }

    NodeP parse_item() {
        Atom atom = parse_atom();
        // Attach scripts/primes: nucleus (_ sub)? (^ sup)? '*
        NodeP sub;
        NodeP sup;
        int primes = 0;
        for (;;) {
            skip_spaces();
            const char32_t c = peek();
            if (c == U'\'') {
                ++pos_;
                ++primes;
                continue;
            }
            if (c != U'_' && c != U'^') {
                break;
            }
            ++pos_;
            skip_spaces();
            ++script_depth_;
            Atom arg = parse_atom();
            --script_depth_;
            if (c == U'_') {
                if (sub) {
                    throw ValueError("mathtext: double subscript");
                }
                sub = std::move(arg.node);
            } else {
                if (sup) {
                    throw ValueError("mathtext: double superscript");
                }
                sup = std::move(arg.node);
            }
        }
        if (!sub && !sup && primes == 0) {
            return std::move(atom.node);
        }
        return subsuper(std::move(atom), std::move(sub), std::move(sup), primes);
    }

    Atom parse_atom() {
        skip_spaces();
        if (eof()) {
            throw ValueError("mathtext: unexpected end of input");
        }
        const char32_t c = peek();
        if (c == U'{') {
            ++pos_;
            std::vector<NodeP> group = parse_list(/*inside_group=*/true);
            return {std::make_unique<Hlist>(std::move(group)), 0, false};
        }
        if (c == U'\\') {
            return parse_command();
        }
        ++pos_;
        return make_symbol(c == U'-' ? char32_t{0x2212} : c); // unary/binary minus sign
    }

    Atom parse_command() {
        ++pos_; // backslash
        if (eof()) {
            throw ValueError("mathtext: lone '\\'");
        }
        std::string name;
        if (is_alpha(peek())) {
            while (!eof() && is_alpha(peek())) {
                name.push_back(static_cast<char>(next()));
            }
        } else {
            name.push_back(static_cast<char>(next())); // \, \; \! etc.
        }

        for (const SpaceCommand& sc : kSpaceCommands) {
            if (sc.name == name) {
                return {std::make_unique<Kern>(sc.em_fraction * em_advance()), 0, false};
            }
        }
        if (name == "frac" || name == "dfrac") {
            return parse_frac(name == "dfrac");
        }
        if (name == "sqrt") {
            return parse_sqrt();
        }
        // Parser._function_names: \sin, \log, \max, ... typeset upright.
        static constexpr std::array<std::string_view, 32> kFunctionNames = {
            "Pr",  "arccos", "arcsin", "arctan", "arg",    "cos", "cosh",   "cot",
            "coth", "csc",   "deg",    "det",    "dim",    "exp", "gcd",    "hom",
            "inf", "ker",    "lg",     "lim",    "liminf", "limsup", "ln",  "log",
            "max", "min",    "sec",    "sin",    "sinh",   "sup", "tan",    "tanh"};
        if (std::find(kFunctionNames.begin(), kFunctionNames.end(), name) !=
            kFunctionNames.end()) {
            std::vector<NodeP> chars;
            for (const char c : name) {
                chars.push_back(std::make_unique<Char>(static_cast<char32_t>(c), em_,
                                                       FaceId::regular, false));
            }
            return {std::make_unique<Hlist>(std::move(chars)), 0, false};
        }
        if (name == "mathrm" || name == "mathbf" || name == "mathit") {
            const FaceId face = name == "mathbf" ? FaceId::bold
                                : name == "mathit" ? FaceId::oblique
                                                   : FaceId::regular;
            return parse_styled_group(face, /*slanted=*/name == "mathit");
        }
        // Everything else must be a plain symbol; anything structural that we
        // do not typeset is named explicitly for the user.
        if (name == "left" || name == "right" || name == "begin" || name == "end" ||
            name == "over" || name == "text" || name == "operatorname") {
            throw ValueError("mathtext: \\" + name + " is not supported by graphlib yet");
        }
        const char32_t cp = lookup_symbol(name);
        return make_symbol(cp, /*force_upright=*/!is_greek_lower(cp));
    }

    Atom parse_group_arg() {
        skip_spaces();
        if (peek() != U'{') {
            // single-token argument, TeX style: \frac12
            return parse_atom();
        }
        ++pos_;
        std::vector<NodeP> group = parse_list(/*inside_group=*/true);
        return {std::make_unique<Hlist>(std::move(group)), 0, false};
    }

    Atom parse_frac(bool display) {
        Atom num = parse_group_arg();
        Atom den = parse_group_arg();
        const double thickness = rule_thickness();

        if (!display) { // mpl _MathStyle.TEXTSTYLE => one shrink
            num.node->shrink();
            den.node->shrink();
        }
        const double target = std::max(num.node->width, den.node->width);
        auto cnum = wrap_hlist(std::move(num.node));
        auto cden = wrap_hlist(std::move(den.node));
        cnum->center_to(target);
        cden->center_to(target);
        const double cden_height = cden->height;

        std::vector<NodeP> stack;
        stack.push_back(std::move(cnum));
        stack.push_back(std::make_unique<Box>(0.0, 0.0, thickness * 2.0));
        stack.push_back(std::make_unique<Rule>(target, thickness / 2.0, thickness / 2.0));
        stack.push_back(std::make_unique<Box>(0.0, 0.0, thickness * 2.0));
        stack.push_back(std::move(cden));
        auto vlist = std::make_unique<Vlist>(std::move(stack));

        // Shift so the bar aligns with the middle of an '=' sign.
        const GlyphMetrics eq =
            FontManager::instance().glyph_metrics(U'=', em_, FaceId::regular);
        vlist->shift = cden_height - ((eq.ymax + eq.ymin) / 2.0 - thickness * 3.0);

        std::vector<NodeP> row;
        row.push_back(std::move(vlist));
        row.push_back(std::make_unique<Box>(thickness * 2.0, 0.0, 0.0));
        return {std::make_unique<Hlist>(std::move(row), /*do_kern=*/false), 0, false};
    }

    Atom parse_sqrt() {
        skip_spaces();
        NodeP root;
        if (peek() == U'[') { // \sqrt[3]{x}
            ++pos_;
            std::vector<NodeP> r;
            while (!eof() && peek() != U']') {
                r.push_back(parse_item());
            }
            if (eof()) {
                throw ValueError("mathtext: expected ']'");
            }
            ++pos_;
            auto h = std::make_unique<Hlist>(std::move(r));
            h->shrink();
            h->shrink();
            root = std::move(h);
        }
        Atom body_atom = parse_group_arg();
        NodeP body = std::move(body_atom.node);
        const double thickness = rule_thickness();

        // Stretch the radical glyph to span the body (AutoHeightChar-lite).
        const double target_height = body->height - body->shift + thickness * 5.0;
        const double target_depth = body->depth + body->shift;
        auto radical = std::make_unique<Char>(0x221A, em_, FaceId::regular, false);
        const double glyph_span = radical->height + radical->depth;
        if (glyph_span > 0.0) {
            radical->y_scale = (target_height + target_depth) / glyph_span;
            radical->update_metrics();
        }
        // AutoHeightChar: shift so the stretched glyph's depth matches the
        // body's; the effective height above baseline is what the rule meets.
        radical->shift = target_depth - radical->depth;
        const double check_width = radical->width;
        const double height = radical->height - radical->shift;

        // Body with side padding, an overline resting on top of it.
        std::vector<NodeP> padded;
        padded.push_back(std::make_unique<Box>(2.0 * thickness, 0.0, 0.0));
        padded.push_back(std::move(body));
        padded.push_back(std::make_unique<Box>(2.0 * thickness, 0.0, 0.0));
        auto padded_body = std::make_unique<Hlist>(std::move(padded), /*do_kern=*/false);
        const double body_width = padded_body->width;

        std::vector<NodeP> right;
        right.push_back(std::make_unique<Rule>(body_width, thickness / 2.0, thickness / 2.0));
        // gap so the rule sits at the radical's top (mpl stretches fill glue)
        right.push_back(std::make_unique<Kern>(height + 0.06 * em_ - thickness -
                                               (padded_body->height - padded_body->shift)));
        right.push_back(std::move(padded_body));
        auto rightside = std::make_unique<Vlist>(std::move(right));

        std::vector<NodeP> row;
        if (root) {
            root->shift = -height * 0.6; // hard-coded in mpl, "a hack ;)"
            row.push_back(std::move(root));
            row.push_back(std::make_unique<Kern>(-check_width * 0.5));
        }
        row.push_back(std::move(radical));
        row.push_back(std::move(rightside));
        return {std::make_unique<Hlist>(std::move(row), /*do_kern=*/false), 0, false};
    }

    Atom parse_styled_group(FaceId face, bool slanted) {
        skip_spaces();
        if (peek() != U'{') {
            throw ValueError("mathtext: \\mathrm-style commands need a {...} argument");
        }
        ++pos_;
        std::vector<NodeP> nodes;
        while (!eof() && peek() != U'}') {
            const char32_t c = next();
            if (c == U' ') {
                continue;
            }
            nodes.push_back(std::make_unique<Char>(c, em_, face, slanted));
        }
        if (eof()) {
            throw ValueError("mathtext: expected '}'");
        }
        ++pos_;
        return {std::make_unique<Hlist>(std::move(nodes)), 0, false};
    }

    static bool is_alpha(char32_t c) {
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
    }
    static bool is_greek_lower(char32_t cp) { return cp >= 0x3B1 && cp <= 0x3C9; }

    Atom make_symbol(char32_t cp, bool force_upright = false) {
        // mathtext.default='it': latin letters and lowercase greek are slanted.
        const bool slanted = !force_upright && (is_alpha(cp) || is_greek_lower(cp));
        const FaceId face = slanted ? FaceId::oblique : FaceId::regular;
        auto ch = std::make_unique<Char>(cp, em_, face, slanted);

        const char32_t prev = prev_symbol_;
        prev_symbol_ = cp;
        // Parser.symbol: binary/relation symbols get 0.2em on both sides,
        // except in scripts or when a binary operator starts an expression.
        if (in_sorted(kSpacedCodepoints, cp)) {
            const bool binary = in_sorted(kBinaryCodepoints, cp);
            const bool suppress =
                script_depth_ > 0 ||
                (binary && (prev == 0 || prev == U'{' || prev == U'(' || prev == U'[' ||
                            in_sorted(kRelationCodepoints, prev)));
            if (!suppress) {
                std::vector<NodeP> spaced;
                spaced.push_back(std::make_unique<Kern>(0.2 * em_advance()));
                spaced.push_back(std::move(ch));
                spaced.push_back(std::make_unique<Kern>(0.2 * em_advance()));
                return {std::make_unique<Hlist>(std::move(spaced)), cp, slanted};
            }
        }
        return {std::move(ch), cp, slanted};
    }

    static std::unique_ptr<Hlist> wrap_hlist(NodeP node) {
        if (auto* h = dynamic_cast<Hlist*>(node.get()); h != nullptr) {
            node.release();
            return std::unique_ptr<Hlist>(h);
        }
        std::vector<NodeP> kids;
        kids.push_back(std::move(node));
        return std::make_unique<Hlist>(std::move(kids), /*do_kern=*/false);
    }

    // Port of Parser.subsuper (regular scripts + over/under for \sum et al.).
    NodeP subsuper(Atom nucleus_atom, NodeP sub, NodeP sup, int primes) {
        const double thickness = rule_thickness();
        const double x_height = FontManager::x_height(em_);

        if (primes > 0) {
            std::vector<NodeP> ps;
            if (sup) {
                ps.push_back(std::move(sup));
            }
            for (int i = 0; i < primes; ++i) {
                ps.push_back(std::make_unique<Char>(0x2032, em_, FaceId::regular, false));
            }
            sup = std::make_unique<Hlist>(std::move(ps));
        }

        NodeP nucleus = std::move(nucleus_atom.node);
        const char32_t ncp = nucleus_atom.cp;

        if (ncp != 0 && contains_cp(kOverunder, ncp)) { // \sum-style limits
            const double vgap = thickness * 3.0;
            double target = nucleus->width;
            if (sup) {
                sup->shrink();
                target = std::max(target, sup->width);
            }
            if (sub) {
                sub->shrink();
                target = std::max(target, sub->width);
            }
            std::vector<NodeP> stack;
            double shift = 0.0;
            if (sup) {
                auto c = wrap_hlist(std::move(sup));
                c->center_to(target);
                stack.push_back(std::move(c));
                stack.push_back(std::make_unique<Box>(0.0, 0.0, vgap));
            }
            {
                auto c = wrap_hlist(std::move(nucleus));
                c->center_to(target);
                stack.push_back(std::move(c));
            }
            const double nucleus_depth = stack.back()->depth;
            if (sub) {
                auto c = wrap_hlist(std::move(sub));
                c->center_to(target);
                shift = c->height + vgap + nucleus_depth;
                stack.push_back(std::make_unique<Box>(0.0, vgap, 0.0));
                stack.push_back(std::move(c));
            }
            auto v = std::make_unique<Vlist>(std::move(stack));
            v->shift = shift;
            std::vector<NodeP> row;
            row.push_back(std::move(v));
            return std::make_unique<Hlist>(std::move(row), /*do_kern=*/false);
        }

        const bool dropsub = ncp != 0 && contains_cp(kDropsub, ncp);
        const bool slanted = nucleus_atom.slanted;
        const double lc_height = nucleus->height;
        const double lc_baseline = dropsub ? nucleus->depth : 0.0;

        double superkern = kDelta * x_height;
        double subkern = kDelta * x_height;
        if (slanted) {
            superkern += kDelta * x_height;
            superkern += kDeltaSlanted * (lc_height - x_height * 2.0 / 3.0);
            if (dropsub) {
                subkern = (3.0 * kDelta - kDeltaIntegral) * lc_height;
                superkern = (3.0 * kDelta + kDeltaIntegral) * lc_height;
            } else {
                subkern = 0.0;
            }
        }

        NodeP x;
        if (!sup) {
            std::vector<NodeP> kids;
            kids.push_back(std::make_unique<Kern>(subkern));
            kids.push_back(std::move(sub));
            auto h = std::make_unique<Hlist>(std::move(kids), /*do_kern=*/false);
            h->shrink();
            h->shift = dropsub ? lc_baseline + kSubdrop * x_height : kSub1 * x_height;
            x = std::move(h);
        } else {
            std::vector<NodeP> kids;
            kids.push_back(std::make_unique<Kern>(superkern));
            kids.push_back(std::move(sup));
            auto hx = std::make_unique<Hlist>(std::move(kids), /*do_kern=*/false);
            hx->shrink();
            double shift_up = dropsub ? lc_height - kSubdrop * x_height : kSup1 * x_height;
            if (!sub) {
                hx->shift = -shift_up;
                x = std::move(hx);
            } else {
                std::vector<NodeP> kids2;
                kids2.push_back(std::make_unique<Kern>(subkern));
                kids2.push_back(std::move(sub));
                auto hy = std::make_unique<Hlist>(std::move(kids2), /*do_kern=*/false);
                hy->shrink();
                const double shift_down =
                    dropsub ? lc_baseline + kSubdrop * x_height : kSub2 * x_height;
                const double clr =
                    2.0 * thickness - ((shift_up - hx->depth) - (hy->height - shift_down));
                if (clr > 0.0) {
                    shift_up += clr;
                }
                std::vector<NodeP> stack;
                const double gap = (shift_up - hx->depth) - (hy->height - shift_down);
                stack.push_back(std::move(hx));
                stack.push_back(std::make_unique<Kern>(gap));
                stack.push_back(std::move(hy));
                auto v = std::make_unique<Vlist>(std::move(stack));
                v->shift = shift_down;
                x = std::move(v);
            }
        }
        if (!dropsub) {
            x->width += kScriptSpace * x_height;
        }

        std::vector<NodeP> row;
        row.push_back(std::move(nucleus));
        row.push_back(std::move(x));
        return std::make_unique<Hlist>(std::move(row), /*do_kern=*/false);
    }
};

} // namespace

bool contains_math(std::string_view utf8) {
    for (size_t i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '$' && (i == 0 || utf8[i - 1] != '\\')) {
            return true;
        }
    }
    return false;
}

std::vector<TextRun> split_math_runs(std::string_view line) {
    std::vector<TextRun> runs;
    std::string current;
    bool math = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\\' && i + 1 < line.size() && line[i + 1] == '$' && !math) {
            current.push_back('$');
            ++i;
            continue;
        }
        if (c == '$') {
            if (!current.empty() || math) {
                runs.push_back({std::move(current), math});
                current.clear();
            }
            math = !math;
            continue;
        }
        current.push_back(c);
    }
    if (math) {
        throw ValueError("mathtext: unmatched '$' in " + std::string(line));
    }
    if (!current.empty()) {
        runs.push_back({std::move(current), false});
    }
    return runs;
}

MathBox layout_math(std::string_view inner, double em_px) {
    const std::vector<char32_t> cps32 = decode_utf8(inner);
    const std::u32string src(cps32.begin(), cps32.end());
    MathParser parser(src, em_px);
    const NodeP root = parser.parse();

    MathBox box;
    box.width = root->width;
    box.height = root->height;
    box.depth = root->depth;
    root->render(box.path, 0.0, 0.0);
    return box;
}

} // namespace graphlib::detail
