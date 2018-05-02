#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>
#include <mutex>
#include <tuple>
#include <memory>

#include "version.h"

using namespace std;

using values_t = vector<string>;
using shuffle_data_t = tuple<unique_ptr<mutex>, values_t >;
using mapper_t = std::function<void(const string &, values_t &)>;
using reducer_t = std::function<void(const values_t &, values_t &)>;

struct slice
{
    int begin_offset;
    int end_offset;
};

int check_console_input(const int mnum, const int rnum, const string &path)
{
    int result = 0;
    if (mnum < 0)
    {
        cout << "error, negative mnum : " << mnum << endl;
        result = 1;
    }

    if (rnum < 0)
    {
        cout << "error, negative rnum : " << rnum << endl;
        result = 1;
    }

    if(path.length() < 1)
    {
        cout << "error, bad path : " << path << endl;
        result = 1;
    }
    return result;
}

int split_file(const string &path, const int slices_count, vector<slice> &offsets)
{
    ifstream f(path, ios::binary | ios::ate);
    if(!f.is_open())
    {
        cout << "error, no such file: " << path << endl;
        return 1;
    }
    auto file_size = f.tellg();
    auto slice_size = file_size/slices_count;
    decltype(file_size) cur_offset = 0;

    offsets.reserve(slices_count);

    f.seekg(cur_offset);
    while (cur_offset < file_size)
    {
        slice slc;
        slc.begin_offset = cur_offset;

        if ((file_size - cur_offset) > slice_size)
        {
            f.seekg(slice_size, ios_base::cur);
            while ((f.get() != '\n') && (f.peek() != EOF));
            cur_offset = f.tellg();
            if(cur_offset == EOF)
            {
                cur_offset = file_size;
            }
        }
        else
        {
            cur_offset = file_size;
        }

        slc.end_offset = cur_offset;
        offsets.push_back(slc);
    }

    f.close();

    return 0;
}

void mapper_thread(const std::string &path_, const slice &slice_, values_t &strings, mapper_t m)
{
    ifstream f(path_, ios_base::in);
    if(!f.is_open())
    {
        cout << "mapper_thread error, can't open file. PID = " << this_thread::get_id() << endl;
    }

    string line;
    f.seekg(slice_.begin_offset);
    while (std::getline(f, line) && (f.tellg() <= slice_.end_offset))
    {
        m(line, strings);
    }

    std::sort(strings.begin(), strings.end());

    f.close();
}

void do_map(const std::string &path_, const vector<slice> &slices_, vector<values_t> &input_strings_, mapper_t m)
{
    input_strings_.reserve(slices_.size());
    vector<thread> mapper_threads;

    for(auto i = 0; i < slices_.size(); ++i)
    {
        input_strings_.push_back(values_t());
        mapper_threads.push_back(thread(mapper_thread, std::ref(path_), std::ref(slices_[i]), std::ref(input_strings_.back()), m));
    }

    for(auto &t : mapper_threads)
    {
        t.join();
    }

    cout << "input_strings_ size = " << input_strings_.size() << " slices " << slices_.size() << endl;
    for(auto vct : input_strings_)
    {
        cout << "container size = " << vct.size()
             << ", front =" << vct.front()
             << ", \t\tback =" << vct.back() << endl;
    }
}

void do_shuffle(int rnum_, const vector<values_t> &strings_, vector<shuffle_data_t> &sdata_)
{
    vector<thread> shuffle_threads;

    sdata_.resize(rnum_);
    for(auto &r : sdata_)
    {
        get<0>(r) = move(make_unique<mutex>());
    }

    for(auto i = 0; i < strings_.size(); ++i)
    {
        const values_t &values = strings_[i];
        shuffle_threads.push_back(
                    thread([rnum_, values, &sdata_]
                            {
                                for(auto s : values)
                                {
                                    int r_id = hash<string>{}(s) % rnum_;
                                    auto &m = get<0>(sdata_[r_id]);
                                    auto &strs = get<1>(sdata_[r_id]);

                                    lock_guard<mutex> lk(*m);
                                    auto it = lower_bound(strs.begin(), strs.end(), s);
                                    strs.insert(it, s);
                                }
                            }
                           )
                    );
    }

    for(auto &t : shuffle_threads)
    {
        t.join();
    }

    for(auto &rd : sdata_)
    {
        auto &values = get<1>(rd);
        cout << "container size = " << values.size()
             << ", front =" << values.front()
             << ", \t\tback =" << values.back() << endl;
    }
}

void do_reduce(int rnum_, const vector<shuffle_data_t> &sdata_, reducer_t r)
{
    vector<thread> reducer_threads;

    for(auto i = 0; i < rnum_; ++i)
    {
        auto &values = get<1>(sdata_[i]);
        reducer_threads.push_back(
                    thread([i, values, r]
                            {
                                values_t result;

                                r(values, result);

                                string filename = "r_" + to_string(i) + ".txt";
                                ofstream f(filename, ios_base::out);

                                //f << result.size() << endl;
                                for(auto &s : result)
                                {
                                    f << s << "\nlen = "<< s.length() << endl;
                                }
                                f.flush();
                                f.close();
                            }
                        )
                    );
    }

    for(auto &t : reducer_threads)
    {
        t.join();
    }
}

int main(int argc, char* argv[])
{
    try
    {
        string path;
        int mnum = 0;
        int rnum = 0;

        if ((argc > 1) &&
                (!strncmp(argv[1], "-v", 2) || !strncmp(argv[1], "--version", 9)))
        {
            cout << "version " << version() << endl;
            return 0;
        }
        else if (argc == 4)
        {
            path = string(argv[1]);
            mnum = atoi(argv[2]);
            rnum = atoi(argv[3]);

            if(check_console_input(mnum, rnum, path))
            {
                return 1;
            }
            cout << "yamr M: " << mnum << ", R: " << rnum << ", path: " << path << endl;
        }
        else
        {
            std::cerr << "Usage: yamr <src> <mnum> <rnum>\n";
            return 1;
        }

        vector<slice> slices;
        if(!split_file(path, mnum, slices))
        {

            //Map
            cout << "Map ->" << endl;
            vector<values_t> input_strings;
            auto mapper = [] (const string &line, values_t &strings)
            {
                strings.push_back(line);
            };
            do_map(/*mnum, */path, slices, input_strings, mapper);

            //Shuffle
            cout << "Shuffle ->" << endl;
            vector<shuffle_data_t> shuffle_data;
            do_shuffle(/*mnum, */rnum, input_strings, shuffle_data);

            //Reduce
            cout << "Reduce ->" << endl;
            auto reducer = [](const values_t &values, values_t &result)
            {
                unique_copy(values.begin(), values.end(), back_inserter(result));
                auto last_found = result.begin();

                for(int i = 1; ; ++i)
                {
                    auto it = adjacent_find(result.begin(), result.end(),
                                       [i](string &first, string &second)
                                        {
                                            return first.substr(0, i) == second.substr(0, i);
                                        }
                                       );
                    if(it != result.end())
                    {
                        last_found = it;
                    }
                    else
                    {
                        break;
                    }
                }
                string unique_prefix_string = *last_found;
                result.clear();
                result.push_back(unique_prefix_string);
            };
            do_reduce(rnum, shuffle_data, reducer);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
