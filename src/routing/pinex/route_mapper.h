#ifndef _ROUTEMAPPER_H_
#define _ROUTEMAPPER_H_
#include <pthread.h>
#include <cstdio>
#include <string>
#include <limits.h>
#include <vector>

namespace peykasa
{
    namespace routing_table
    {
        /** Declaration **/

        template<class T>
        struct route_data
        {
            std::string prefix_;
            T destination_;
            unsigned int max_length_;

            route_data()
            {
                max_length_ = UINT_MAX;
            }
        };

        template<class T>
        class char_node
        {
        public:
            char_node(int depth, char_node<T>* parent, unsigned int index);
            virtual ~char_node();

            void clean_node();

            T destination_;
            unsigned int index_;
            unsigned int max_length_;
            bool has_value_flag_;
            unsigned int depth_;
            unsigned int number_of_childs_;
            char_node<T>* childs_[256];
            char_node<T>* parent_;
        };

        template<class T>
        class route_mapper
        {
        public:
            route_mapper();
            virtual ~route_mapper();

            bool add_route(std::vector<route_data<T> >& routes, bool clear = false);

            bool add_route(const std::string& prefix, T destination, unsigned int maxLength = UINT_MAX);
            bool update_route(const std::string& prefix, T destination, unsigned int maxLength = UINT_MAX);
            bool delete_route(const std::string& prefix);
            void clear_all_routes();

            T find_destination(const std::string& prefix, bool reverse = false);

        private:
            pthread_rwlock_t mutex_;

            char_node<T>* tree_root_;

            void clean_up(char_node<T>* current);

            char_node<T>* create_sub_tree(const std::string& prefix,
                                          unsigned int       first,
                                          unsigned int       last,
                                          char_node<T>*      current);

            char_node<T>* find_matching(const std::string& prefix,
                                        unsigned int       first,
                                        unsigned int       last,
                                        char_node<T>*      current,
                                        char_node<T>*      lastValidValue,
                                        bool               reverse);
        };


        /** Implementation **/

        template<class T> char_node<T>::char_node(int depth, char_node<T>* parent, unsigned int index)
        {
            has_value_flag_ = false;
            parent_ = parent;
            depth_ = depth;
            index_ = index;
            number_of_childs_ = 0;

            for(int i = 0; i < 256; i++)
            {
                childs_[i] = NULL;
            }
        }

        template<class T> char_node<T>::~char_node()
        {
        }

        template<class T>
        void char_node<T>::clean_node()
        {
            for(int i = 0; i < 256; i++)
            {
                if(childs_[i] != NULL)
                {
                    childs_[i]->clean_node();
                    delete childs_[i];
                    childs_[i] = NULL;
                }
            }

            number_of_childs_ = 0;
        } //>::clean_node

        template<class T> route_mapper<T>::route_mapper()
        {
            tree_root_ = new char_node<T>(0, NULL, -1);
            pthread_rwlock_init(&mutex_, NULL);
        }

        template<class T> route_mapper<T>::~route_mapper()
        {
            delete tree_root_;
            pthread_rwlock_destroy(&mutex_);
        }

        template<class T>
        bool route_mapper<T>::add_route(const std::string& prefix, T destination, unsigned int maxLength)
        {
            bool res;
            pthread_rwlock_wrlock(&mutex_);
            char_node<T>* n = create_sub_tree(prefix, 0, prefix.length(), tree_root_);

            if(n != NULL)
            {
                n->destination_ = destination;
                n->max_length_ = maxLength;
                n->has_value_flag_ = true;
                res = true;
            }
            else
            {
                res = false;
            }

            pthread_rwlock_unlock(&mutex_);
            return res;
        } //>::add_route

        template<class T>
        bool route_mapper<T>::add_route(std::vector<route_data<T> >& routes, bool clear)
        {
            bool res = true;
            pthread_rwlock_wrlock(&mutex_);

            if(clear)
            {
                tree_root_->clean_node();
            }

            for(int i = 0; i < routes.size(); i++)
            {
                char_node<T>* n = create_sub_tree(routes[i].prefix_, 0, routes[i].prefix_.length(), tree_root_);

                if(n != NULL)
                {
                    n->destination_ = routes[i].destination_;
                    n->max_length_ = routes[i].max_length_;
                    n->has_value_flag_ = true;
                }
                else
                {
                    //LOG_ERROR("Add Route Failed! A Route For {} Already Exists!", routes[i].prefix_.c_str());
                    res = false;
                }
            }

            pthread_rwlock_unlock(&mutex_);
            return res;
        } //>::add_route

        template<class T>
        bool route_mapper<T>::update_route(const std::string& prefix, T destination, unsigned int maxLength)
        {
            bool res;
            pthread_rwlock_wrlock(&mutex_);
            char_node<T>* n = find_matching(prefix, 0, prefix.length(), tree_root_, NULL, false);

            if((n != NULL) && (n->depth_ == prefix.length()))
            {
                n->destination_ = destination;
                n->max_length_ = maxLength;
                res = true;
            }
            else
            {
                //LOG_ERROR("Update Route Failed! No Route For {} Exists!", (char*) prefix.c_str());
                res = false;
            }

            pthread_rwlock_unlock(&mutex_);
            return res;
        } //>::update_route

        template<class T>
        bool route_mapper<T>::delete_route(const std::string& prefix)
        {
            bool res;
            pthread_rwlock_wrlock(&mutex_);
            char_node<T>* n = find_matching(prefix, 0, prefix.length(), tree_root_, NULL, false);

            if((n != NULL) && (n->depth_ == prefix.length()))
            {
                n->has_value_flag_ = false;
                clean_up(n);
                res = true;
            }
            else
            {
                //LOG_ERROR("Delete Route Failed! No Route For {} Exists!", (char*) prefix.c_str());
                res = false;
            }

            pthread_rwlock_unlock(&mutex_);
            return res;
        } //>::delete_route

        template<class T>
        void route_mapper<T>::clear_all_routes()
        {
            pthread_rwlock_wrlock(&mutex_);
            tree_root_->clean_node();
            pthread_rwlock_unlock(&mutex_);
        }

        template<class T>
        T route_mapper<T>::find_destination(const std::string& prefix, bool reverse)
        {
            T res;
            pthread_rwlock_rdlock(&mutex_);
            char_node<T>* n = find_matching(prefix, 0, prefix.length(), tree_root_, NULL, reverse);

            if((n != NULL))
            {
                if(n->max_length_ < prefix.length())
                {
                    //LOG_DEBUG("No Route Found For {} According To Max Length Constraint!", prefix.c_str());
                    res = "";
                }
                else
                {
                    res = n->destination_;
                }
            }
            else
            {
                //LOG_DEBUG("No Route For {} Found!", prefix.c_str());
                res = "";
            }

            pthread_rwlock_unlock(&mutex_);
            return res;
        } //>::find_destination

        template<class T>
        char_node<T>* route_mapper<T>::find_matching(const std::string& prefix,
                                                     unsigned int       first,
                                                     unsigned int       last,
                                                     char_node<T>*      current,
                                                     char_node<T>*      lastValidValue,
                                                     bool               reverse)
        {
            unsigned char index;

            if(current->has_value_flag_)
            {
                lastValidValue = current;
            }

            //currently last point to end of string
            if(reverse && last)
            {
                last--;
            }

            if(first < last)
            {
                if(reverse)
                {
                    index = prefix[last];
                }
                else
                {
                    index = prefix[first];
                }

                if(current->childs_[index] != NULL)
                {
                    if(reverse)
                    {
                        return (find_matching(prefix, first, last, current->childs_[index], lastValidValue, reverse));
                    }
                    else
                    {
                        return (find_matching(prefix, first + 1, last, current->childs_[index], lastValidValue, reverse));
                    }
                }
                else
                {
                    return lastValidValue;
                }
            }
            else
            {
                return lastValidValue;
            }
        } //>::find_matching

        template<class T>
        char_node<T>* route_mapper<T>::create_sub_tree(const std::string& prefix,
                                                       unsigned int       first,
                                                       unsigned int       last,
                                                       char_node<T>*      current)
        {
            if(first < last)
            {
                unsigned char index = prefix[first];

                if(current->childs_[index] == NULL)
                {
                    current->childs_[index] = new char_node<T>(current->depth_ + 1, current, index);
                    current->number_of_childs_++;
                }

                return (create_sub_tree(prefix, first + 1, last, current->childs_[index]));
            }
            else
            {
                if(current->has_value_flag_)
                {
                    return NULL;
                }
                else
                {
                    return current;
                }
            }
        } //>::create_sub_tree

        template<class T>
        void route_mapper<T>::clean_up(char_node<T>* current)
        {
            if((!current->has_value_flag_) && (!current->number_of_childs_))
            {
                char_node<T>* parent = current->parent_;

                if(parent != NULL)
                {
                    parent->number_of_childs_--;
                    parent->childs_[current->index_] = NULL;
                    delete current;
                    clean_up(parent);
                }
            }
        } //>::clean_up
    }
}

using namespace peykasa::routing_table;
#endif //ifndef _route_mapper_H_
