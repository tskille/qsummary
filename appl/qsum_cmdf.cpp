/*
   Copyright 2022 Equinor ASA.

   This file is part of the Open Porous Media project (OPM).

   OPM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   OPM is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with OPM.  If not, see <http://www.gnu.org/licenses/>.
   */

#include <appl/qsum_cmdf.hpp>

#include <iostream>

#include <filesystem>
#include <fstream>


QsumCMDF::QsumCMDF(const std::string& cmd_file, int num_smry_files)

{
    m_num_smry_files = num_smry_files;

    // reading command file, no processing stored in variable m_cmd_lines
    get_cmdlines(cmd_file);

    // variable $NUM_CASES is processed if used, stored in variable m_cmd_lines
    //  > in future add variable $CMDL_LIST which is a list submitted on the command line
    update_variables();

    // processing cmd_lines storing this in m_processed_cmd_lines
    // for loops and lists are processed
    m_processed_cmd_lines = process_cmdlines();

    update_rhs_cmd_lines_define();

    make_define_vect();
}


void QsumCMDF::print_m_defined()
{
    std::cout << "\n\nQsumCMDF::print_m_defined: \n\n";

    for (size_t n=0; n < m_define_vect.size(); n++){
        auto def =  m_define_vect[n];

        std::cout << n << " " << std::get<0>(def);
        std::cout << " " << std::get<1>(def);
        std::cout << " " << std::get<2>(def);

        std::cout << "\n";
    }
}


int QsumCMDF::derived_key_index(const std::string& name)
{

    for (size_t n = 0; n < m_define_vect.size(); n++){
        auto def =  m_define_vect[n];
        std::string def_name = std::get<0>(def);

        if (def_name == name )
            return n;
    }

    return -1;
}


bool QsumCMDF::is_number(const std::string& numstr)
{
    for (char const &c : numstr)
        if (std::isdigit(c) == 0)
            return false;

    return true;
}


void QsumCMDF::make_define_vect()
{
    for (size_t n = 0; n < m_processed_cmd_lines.size(); n++){

        auto tokens = split(m_processed_cmd_lines[n], " \t");
        if (tokens[0] == "DEFINE"){

            auto name = tokens[1];
            auto unit = tokens[2];
            std::string expr;

            for (size_t n = 4; n < tokens.size(); n++)
                expr = expr + tokens[n] + " ";

            expr.pop_back();

            std::tuple<std::string, std::string, std::string> def = std::make_tuple(name, expr, unit);

            int key_index = derived_key_index(name);

            if (key_index > -1)
                m_define_vect.erase(m_define_vect.begin() + key_index);

            m_define_vect.push_back(def);
        }
    }
}


void QsumCMDF::get_cmdlines(const std::string& filename)
{
    std::filesystem::path fs_cmdf(filename);
    auto file_size = std::filesystem::file_size(fs_cmdf);

    char* buffer = new char [file_size];

    std::ifstream cmdf(filename);
    cmdf.read (buffer, file_size);
    cmdf.close();

    std::string fileStr = std::string(buffer, file_size);

    delete[] buffer;

    bool end_of_file = false;

    int p0 = 0;

    while (! end_of_file) {
        int p1 = fileStr.find_first_of("\n", p0);

        std::string tmp = fileStr.substr(p0, p1-p0);

        remove_trailing_char(tmp," \t");

        if ((tmp.substr(0,2) != "--") and (tmp.size() > 0) and (tmp.substr(0,1) != "#"))
            m_cmd_lines.push_back(tmp);

        if (p1 >  fileStr.size())
            end_of_file = true;

        p0 = p1 + 1;
    }
}


std::vector<std::string> QsumCMDF::split(const std::string& line, const std::string& delim)
{
    std::vector<std::string> res;

    int p0 = 0;
    bool end_of_line = false;

    while (!end_of_line) {

        int p1 = line.find_first_of(delim, p0);

        if (p1 != std::string::npos ) {
            std::string tmp = line.substr(p0, p1-p0);
            if (tmp.size()>0)
                res.push_back(tmp);
        } else {
            end_of_line = true;
            std::string tmp = line.substr(p0);
            if (tmp.size()>0)
                res.push_back(line.substr(p0));
        }

        p0 = p1 + 1;
    }

    return res;
}


void QsumCMDF::remove_trailing_char(std::string& line, const std::string& charlist)
{
    size_t n = line.size();

    while ((n > 0) && (charlist.find(line.substr(n-1, 1)) != std::string::npos))
       n--;

    line = line.substr(0,n);
}


bool QsumCMDF::update_variables()
{
    for (size_t n = 0; n < m_cmd_lines.size(); n++){
        auto pos = m_cmd_lines[n].find("$NUM_CASES");

        while (pos != std::string::npos){
            m_cmd_lines[n].replace(pos,10, std::to_string(m_num_smry_files));
            pos = m_cmd_lines[n].find("$NUM_CASES");
        }
    }

    return true;
}


void QsumCMDF::add_series(input_list_type& input_charts, int smry_ind, const std::string& name, int axis_ind,
                const std::string &xrange_input, bool is_derived)
{

    int chart_ind = input_charts.size() - 1;

    std::tuple<int, std::string, int, bool> new_smry_vect = std::make_tuple(smry_ind, name, axis_ind, is_derived);

    char_input_type chart_input = input_charts.back();

    std::vector<vect_input_type> smry_vect_input = std::get<0>(chart_input);

    smry_vect_input.push_back(new_smry_vect);

    chart_input = std::make_tuple(smry_vect_input, xrange_input);

    input_charts.back() = chart_input;
}


void QsumCMDF::make_charts_from_cmd(input_list_type& input_charts, const std::string xrange_str )
{

    int chart_ind = -1;

    for (auto const &line : m_processed_cmd_lines){

        auto tokens = split(line, " \t");

        if (tokens[0] == "ADD"){

            if ((tokens[1] != "CHART") && (tokens[1] != "SERIES")){
                std::cout << "error processing command file, first arg in ADD should be CHART or SERIES \n\n";
                exit(1);
            }

            if (tokens[1] == "CHART"){

                chart_ind++;

            input_charts.push_back({});

            } else if (tokens[1] == "SERIES"){

                if (chart_ind < 0){
                    std::cout << "error processing command file, ADD chart before adding SERIES \n";
                    exit(1);
                }

                int smry_case = std::stoi(tokens[2]) - 1 ;

                if (smry_case > (m_num_smry_files -1)){
                    std::cout << "\n!Error processing command file: \n\nline > " << line << "\n";
                    std::cout << "\nNumber of cases loaded less than " << smry_case + 1 << "\n\n";
                    exit(1);
                }

                int axis_ind = -1;

                if (tokens.size() > 4)
                    axis_ind = std::stoi(tokens[4]);

                std::string derived_key = std::to_string(smry_case + 1) + ":" + tokens[3];

                bool is_derived = derived_key_index(derived_key) > -1 ? true : false ;

                add_series(input_charts, smry_case, tokens[3], axis_ind, xrange_str, is_derived);
            }
        }
    }
}


std::string QsumCMDF::expand_line_add_series(const std::vector<std::string>& tokens, int smry_ind)
{
    std::string new_str = tokens[0] + " " + tokens[1];
    new_str = new_str + " " + std::to_string(smry_ind + 1);

    for (size_t m = 3; m < tokens.size(); m++)
        new_str = new_str + " " + tokens[m];

    return new_str;
}

std::string QsumCMDF::expand_line_define(const std::vector<std::string>& tokens, int smry_ind)
{
    std::string new_str = tokens[0] + " ";

    std::string var_str = tokens[1];
    var_str.replace(0,1,std::to_string(smry_ind + 1));

    if (tokens[2] == "=")
        new_str = new_str + " " + var_str + " None =";
    else
        new_str = new_str + " " + var_str + " " + tokens[2];

    for (size_t m = 3; m < tokens.size(); m++)
        new_str = new_str + " " + tokens[m];

    return new_str;
}


std::vector<std::string> QsumCMDF::process_range(std::string& line, bool replace_on_line)
{
    auto start = line.find ( "RANGE(" );
    auto end = line.find ( ")", start );

    std::string range_str = line.substr ( start, start - end );

    auto p1 = range_str.find("(");
    auto p2 = range_str.find(",");
    auto p3 = range_str.find(")");

    int from = std::stoi(range_str.substr(p1+1, p2-p1-1));
    int to = std::stoi(range_str.substr(p2+1, p3-p2-1));

    std::vector<std::string> str_list;

    for (int n = from; n < (to + 1); n++)
        str_list.push_back(std::to_string(n));

    if (replace_on_line){
        std::string rep_str;

        for (auto& element: str_list)
            rep_str = rep_str + " " +  element;

        line.replace(start, end, rep_str);
    }

    return str_list;
}


int QsumCMDF::replace_all(std::string& line, const std::string& repstr1, const std::string& repstr2, const std::string& newstr)
{
    int count = 0;

    for (auto& repstr : {
                repstr1, repstr2
            }) {

        int p1 = line.find(repstr);

        while ( p1 != std::string::npos ) {

            line.replace(p1, repstr.size(), newstr);
            count ++;

            p1 = line.find(repstr);
        }
    }

    return count;
}


void QsumCMDF::update_rhs_cmd_lines_define()
{
    for (size_t n = 0; n < m_processed_cmd_lines.size(); n++) {

        auto tokens = split(m_processed_cmd_lines[n], ", \t");

        int p = 0;

        if (tokens[0] == "DEFINE") {
            auto p_smry_id = tokens[1].find(":");
            int smry_id = std::stoi(tokens[1].substr(0,p_smry_id));
            std::string smry_id_str = std::to_string(smry_id) + ":";

            p = m_processed_cmd_lines[n].find_first_of("=");

            while (p != std::string::npos) {

                p = m_processed_cmd_lines[n].find_first_of("$", p + 1);
                int p2;
                bool space_delim = false;
                if (p != std::string::npos){

                    int p_end;

                    if (m_processed_cmd_lines[n].substr(p+1,1) == "{"){
                        // serarch for first of }
                        p_end = m_processed_cmd_lines[n].find_first_of("}", p + 1);

                        if (p_end == std::string::npos)
                            throw std::invalid_argument("syntax error in DEFINE keyword, missing enclosing curly bracket");
                    } else {
                        // serarch for first of space
                        space_delim = true;
                        p_end = m_processed_cmd_lines[n].find_first_of(" ", p + 1);
                    }

                    std::string arg;

                    if (p_end == std::string::npos)
                        arg = m_processed_cmd_lines[n].substr(p);
                    else
                        arg = m_processed_cmd_lines[n].substr(p, p_end - p);

                    if (arg.substr(0, 2) == "${")
                        arg = arg.substr(2);
                    else
                        arg = arg.substr(1);

                    p_smry_id = arg.find(":");

                    if ((p_smry_id == std::string::npos) || (!is_number(arg.substr(1, p_smry_id - 1))))
                        arg = smry_id_str + arg;

                    arg = "${" + arg + "}";

                    if (space_delim)
                        m_processed_cmd_lines[n].replace(p, p_end-p, arg);
                    else
                        m_processed_cmd_lines[n].replace(p, p_end-p + 1, arg);
                }
            }
        }
    }
}


std::vector<std::string> QsumCMDF::process_cmdlines()
{
   // process LIST and FOR keywords

    std::vector<std::string> mod_cmd_lines;

    std::vector<std::vector<std::string>> list_vect;
    std::vector<std::string> list_names;

    size_t lnr = 0;

    while (lnr <  (m_cmd_lines.size())){

        auto tokens = split(m_cmd_lines[lnr], ", \t");

        if (tokens[0] == "LIST") {

            if (tokens.size() < 4) {
                std::cout << "error processing command file, list needs at least 3 arguments \n";
                exit(1);
            }

            if ((tokens[1] != "NEW") && (tokens[1] != "ADD")) {
                std::cout << "error processing command file, first arg in LIST should be NEW or ADD \n";
                exit(1);
            }

            std::string name = tokens[2];
            int list_ind = -1;
            std::vector<std::string>::iterator itr = std::find(list_names.begin(), list_names.end(), name);

            if (tokens[1] == "NEW") {

                if (itr != list_names.end()) {

                    list_ind = std::distance(list_names.begin(), itr);
                    list_vect[list_ind].clear();

                } else {
                    list_names.push_back(name);
                    list_vect.push_back({});
                    list_ind = list_names.size() -1;
                }

                if (tokens[3].substr(0,6) == "RANGE("){

                    auto range_list = process_range(m_cmd_lines[lnr], true);
                    tokens = split(m_cmd_lines[lnr], ", \t");
                }

                for (size_t n = 3; n < tokens.size(); n++)
                    list_vect[list_ind].push_back(tokens[n]);

            } else if (tokens[1] == "ADD") {

                if (itr == list_names.end()) {
                    std::cout << "error processing command file, LIST + ADD, list not found \n";
                    exit(1);
                }

                list_ind = std::distance(list_names.begin(), itr);

                for (size_t n = 3; n < tokens.size(); n++)
                    list_vect[list_ind].push_back(tokens[n]);
            }

        } else if (tokens[0] == "FOR"){

            std::string var_name = tokens[1];
            std::string list_str = tokens[3];

            if (list_str.substr(0,6) == "RANGE(") {

                // make new list and add this to list_vect
                // list names = DUMMY_LIST_X, when x is the first available integer

                auto range_list = process_range(m_cmd_lines[lnr], false);

                int tmp_num = 0;
                std::string tmp_list_name = "DUMMY_LIST_" + std::to_string(tmp_num);

                while (std::find(list_names.begin(), list_names.end(), tmp_list_name) != list_names.end()) {
                    tmp_num ++;
                    tmp_list_name = "DUMMY_LIST_" + std::to_string(tmp_num);
                }

                list_names.push_back(tmp_list_name);
                list_vect.push_back(range_list);

                list_str = tmp_list_name;

            } else {

                if (list_str.substr(0,2) == "${") {
                    list_str = list_str.substr(2);
                    list_str.pop_back();
                } else if (list_str[0] == '$') {
                    list_str = list_str.substr(1);
                }
            }

            int list_ind = -1;

            std::vector<std::string>::iterator itr = std::find(list_names.begin(), list_names.end(), list_str);
            if (itr != list_names.end()) {

                list_ind = std::distance(list_names.begin(), itr);

            } else {
                std::string message("Error processing command file.");
                message = message + " List '" + list_str + "' not found.";

                throw std::runtime_error(message);
            }

            lnr++;
            auto tokens = split(m_cmd_lines[lnr], ", \t");

            std::vector<std::string> loop_lines;

            while (tokens[0] != "NEXT") {

                if (tokens[0] != "NEXT"){

                    if ((tokens.size() > 2) && (tokens[0] == "DEFINE")) {

                        if ( (tokens[1].substr(0,1) == "*") || (tokens[1].substr(0,1) == "?")) {
                            for (size_t n = 0; n < m_num_smry_files; n++)
                                loop_lines.push_back(expand_line_define(tokens, n));
                        } else {

                            if ((tokens[1].substr(1,1) != ":") && (tokens[1].substr(0,1) != "$")){
                                auto p = m_cmd_lines[lnr].find(tokens[1]);
                                m_cmd_lines[lnr].insert(p, "0:");
                            }

                            if (tokens[2] == "="){
                                auto p = m_cmd_lines[lnr].find(tokens[2]);
                                m_cmd_lines[lnr].insert(p, "None ");
                            }

                            loop_lines.push_back(m_cmd_lines[lnr]);
                        }

                    } else if ((tokens.size() > 2) && (tokens[0] == "ADD") && (tokens[1] == "SERIES") &&
                        ( (tokens[2] == "*") || (tokens[2] == "?"))) {

                        for (size_t n = 0; n < m_num_smry_files; n++)
                            loop_lines.push_back(expand_line_add_series(tokens, n));

                    } else {
                        loop_lines.push_back(m_cmd_lines[lnr]);
                    }
                }

                lnr++;
                tokens = split(m_cmd_lines[lnr], ", \t");
            }
            // <-- loop_lines

            for (auto var : list_vect[list_ind]) {
                for (auto v : loop_lines) {
                    std::string repstr1 = "${" + var_name + "}";
                    std::string repstr2 = "$" + var_name;
                    int nrep = replace_all(v, repstr1, repstr2, var);
                    auto p1 = v.find_first_not_of(" \n");
                    mod_cmd_lines.push_back(v.substr(p1));
                }
            }

        } else {

            if ((tokens.size() > 2) && (tokens[0] == "ADD") && (tokens[1] == "SERIES") &&
                    ( (tokens[2] == "*") || (tokens[2] == "?"))) {

                for (size_t n = 0; n < m_num_smry_files; n++)
                    mod_cmd_lines.push_back( expand_line_add_series(tokens, n));

            } else if ((tokens.size() > 2) && (tokens[0] == "DEFINE")) {

                if ( (tokens[1].substr(0,1) == "*") || (tokens[1].substr(0,1) == "?")) {
                    for (size_t n = 0; n < m_num_smry_files; n++)
                        mod_cmd_lines.push_back( expand_line_define(tokens, n));
                } else {

                    if (tokens[1].substr(1,1) != ":"){
                        auto p = m_cmd_lines[lnr].find(tokens[1]);
                        m_cmd_lines[lnr].insert(p, "0:");
                    }

                    if (tokens[2] == "=") {
                        auto p = m_cmd_lines[lnr].find(tokens[2]);
                        m_cmd_lines[lnr].insert(p, "None ");
                    }

                    mod_cmd_lines.push_back(m_cmd_lines[lnr]);
                }

            } else {

                int p1 = m_cmd_lines[lnr].find_first_not_of(" \n");
                mod_cmd_lines.push_back(m_cmd_lines[lnr].substr(p1));
            }
        }

        lnr++;
    }

    return mod_cmd_lines;
}


void QsumCMDF::print_cmd_lines()
{
    for (auto& line : m_cmd_lines)
        std::cout << line << std::endl;
}


void QsumCMDF::print_processd_cmd_lines()
{
    for (auto& line : m_processed_cmd_lines)
        std::cout << line << std::endl;
}


