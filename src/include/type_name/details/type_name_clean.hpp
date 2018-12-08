#pragma once
#include <string>
#include <deque>
#include "type_name/details/type_name_full.hpp"
#include "type_name/details/fp_polyfill/fp_additions.hpp"

namespace type_name
{

// TODO add namespace internal !
 
inline fp::fp_add::tree_separators make_template_tree_separators()
{
    fp::fp_add::tree_separators sep;
    sep.open_child = '<';
    sep.close_child = '>';
    sep.separate_siblings = ',';
    return sep;
}
inline fp::fp_add::show_tree_options make_template_show_tree_options()
{
    fp::fp_add::show_tree_options result;
    result.add_new_lines = false;
    result.add_space_between_siblings = true;
    result.indent = "";
    return result;
}


inline fp::fp_add::string_tree parse_template_tree(const std::string &s)
{
    return parse_string_tree(s, make_template_tree_separators(), false, false);
}

inline fp::fp_add::string_tree filter_undesirable_template_leafs(const fp::fp_add::string_tree &xs)
{
    std::function<bool(const std::string &)> is_node_desirable =
        [](const std::string &nodename) {
            std::vector<std::string> undesirable_nodes = {
                  "std::char_traits"
                , "struct std::char_traits"
                , "std::allocator"
                , "class std::allocator"
                , "std::less"
                };
            bool found =
                std::find(undesirable_nodes.begin(), undesirable_nodes.end(), fp::trim(' ', nodename)) != undesirable_nodes.end();
            return !found;
        };

    auto xss = fp::fp_add::tree_keep_if(is_node_desirable, xs);
    return xss;
}

struct demangle_typename_params
{
    bool remove_std = false;
};

inline std::string perform_replacements(
    const std::string & type_name,
    const std::vector<std::pair<std::string, std::string>> replacements)
{
    std::string result = type_name;
    for (const auto &r : replacements)
        result = fp::replace_tokens(r.first, r.second, result);
    return result;
}


inline std::string remove_extra_namespaces(const std::string & type_name)
{
    return perform_replacements(type_name,
    {
          { "::__1", "" }
        , { "::__cxx11", "" }
    });
}

inline std::string remove_struct_class(const std::string & type_name)
{
    return perform_replacements(type_name,
    {
         { "struct ", "" }
        ,{ "class ", "" }
    });
}

inline std::string perform_std_replacements(const std::string & type_name)
{
    return perform_replacements(type_name,
    {
          {"std::basic_string<char>", "std::string"}
        , {"std::basic_string <char>", "std::string"}
        , {"std::basic_string &<char>", "std::string &"}
        //, {"std::basic_string & const<char>", "std::string& const"}
        , { "std::basic_string const &<char>", "std::string const &" }          
        , {"class std::basic_string<char>", "std::string"}
        , { "> >", ">>" }
    });
}

inline std::vector<std::string> _split_string(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

inline void trim_spaces_inplace(std::string & xs_io)
{
    xs_io = fp::trim(' ', xs_io);
}

// "const T &" should become "T const &"
inline std::string DISABLED_apply_east_const(const std::string & type_name)
{
    return type_name;
    // Examples:
    // Case 1: "const std::string" => "std::string const"
    // Case 2: "const std::string&" => "std::string const &"
    auto typename_ref_attached = perform_replacements(type_name, {
        { " &", "&" },
        { "& ", "&" }
    });

    auto words = _split_string(typename_ref_attached, ' ');

    if (words.empty())
        return "";

    std::deque<std::string> reordered_words;
    for (const auto & word : words)
        reordered_words.push_back(word);

    {
        if (words.front() == "const")
        {
            // Take care of case 1:
            // Case 1: "const std::string" => "std::string const"
            reordered_words.pop_front();
            reordered_words.push_back("const");

            // Take care of case 2:
            // Case 2: "const std::string&" => "std::string const &"
            if (reordered_words.size() >= 2)
            {
                auto & word_last = reordered_words.back();
                auto & word_before_last = reordered_words[reordered_words.size() - 2];
                if (word_before_last.back() == '&')
                {
                    word_before_last.pop_back();
                    word_last.push_back(' '); word_last.push_back('&');
                }
            }
        }
    }

    std::string out = fp::join(std::string(" "), reordered_words);
    return out;
}


inline std::string add_space_before_ref(const std::string & type_name)
{
    std::string result = "";
    bool space_before = false;
    for (auto c : type_name)
    {
        if (c == '&')
            if (!space_before)
                result = result + " ";
        space_before = (c == ' ');
        result = result + c;
    }
    return result;
}


inline std::string clean_typename(const std::string & type_name_)
{
    std::string type_name = fp::trim(' ', type_name_);

    std::string type_name_cleaned = remove_struct_class(remove_extra_namespaces(type_name));

    fp::fp_add::string_tree template_tree = parse_template_tree(type_name_cleaned);
    fp::fp_add::tree_transform_leafs_depth_first_inplace(trim_spaces_inplace, template_tree);
    fp::fp_add::string_tree template_tree_filtered = filter_undesirable_template_leafs(template_tree);

    auto template_tree_filtered_str = fp::fp_add::show_tree(
        template_tree_filtered, 
        make_template_tree_separators(), 
        make_template_show_tree_options());

    std::string final_type = perform_std_replacements(template_tree_filtered_str);
    final_type = add_space_before_ref(final_type);
    //final_type = DISABLED_apply_east_const(final_type);
    return final_type;
}


#define var_type_name_clean(var) type_name::clean_typename( var_type_name_full(var) )

// Also displays the variable name
#define var_name_type_name_clean(var) std::string("type_clean(") + #var + ") = " + var_type_name_clean(var)
#define log_var_name_type_name_clean(var) std::cout << var_name_type_name_clean(var) << std::endl;

#define log_var_str(var) \
        std::string("[") + var_type_name_clean(var) + "] " + #var \
        + " = " \
        + fp::show(var)


#define log_var_str_cont(var) \
        std::string("[") + var_type_name_clean(var) + "] " + #var \
        + " = " \
        + fp::show_cont(var)


#define log_var(var) std::cout << log_var_str(var) << std::endl;
#define log_var_cont(var) std::cout << log_var_str_cont(var) << std::endl;
} // namespace type_name