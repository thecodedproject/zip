#include <tuple>
#include <list>
#include <forward_list>
#include <vector>
#include <iterator>
#include <iostream>
#include <type_traits>

#include <boost/mpl/int.hpp>
#include <boost/mpl/min_element.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/range.hpp>

boost::mpl::int_<1> tag_to_rank(std::input_iterator_tag);
boost::mpl::int_<1> tag_to_rank(std::output_iterator_tag);
boost::mpl::int_<1> tag_to_rank(std::forward_iterator_tag);
boost::mpl::int_<2> tag_to_rank(std::bidirectional_iterator_tag);
boost::mpl::int_<3> tag_to_rank(std::random_access_iterator_tag);

std::forward_iterator_tag rank_to_tag(boost::mpl::int_<1>);
std::bidirectional_iterator_tag rank_to_tag(boost::mpl::int_<2>);
std::random_access_iterator_tag rank_to_tag(boost::mpl::int_<3>);

template <typename... Tags>
struct WeakestIteratorTag
{
  using tag_ranks = typename boost::mpl::vector<decltype(tag_to_rank(
      std::declval<Tags>()))...>::type;

  using min_rank_it = typename boost::mpl::min_element<tag_ranks>::type;
  using min_rank = typename boost::mpl::deref<min_rank_it>::type;

  using tag = decltype(rank_to_tag(std::declval<min_rank>()));
};

template <typename T>
struct HasSizeFunction
{
private:
  template <typename Range>
  static auto test(int) -> decltype(std::declval<Range>().size(), std::true_type());

  template <typename OtherTypes>
  static auto test(...) -> std::false_type;

public:

  using value = decltype(test<T>(0));
};

template <typename... Iterators>
struct ZipIteratorTraits
{
  using value_type = std::tuple<typename std::iterator_traits<Iterators>::value_type...>;

  using reference = std::tuple<typename std::iterator_traits<Iterators>::reference...>;

  using iterator_category = typename WeakestIteratorTag<
    typename std::iterator_traits<Iterators>::iterator_category...>::tag;
};

template<typename... Iterators>
class ZipIterator;

template <typename... Iterators>
using ZipIteratorFacade = boost::iterator_facade<
  ZipIterator<Iterators...>,
  typename ZipIteratorTraits<Iterators...>::value_type,
  typename ZipIteratorTraits<Iterators...>::iterator_category,
  typename ZipIteratorTraits<Iterators...>::reference>;

template <typename... Iterators>
class ZipIterator : public ZipIteratorFacade<Iterators...>
{
friend class boost::iterator_core_access;
public:

  //iterator_tuple = std::tuple<Iterators...>;
  //...

  using value_type = typename ZipIteratorTraits<Iterators...>::value_type;

  using reference = typename ZipIteratorTraits<Iterators...>::reference;

  using iterator_category = typename ZipIteratorTraits<Iterators...>::iterator_category;

  ZipIterator(Iterators&&... iterators) :
    iterator_tuple_(std::forward<Iterators>(iterators)...)
  {
  };

private:

  void increment();
  void decrement();

  bool equal(ZipIterator const& rhs) const;

  auto dereference() const -> reference;

  std::tuple<Iterators...> iterator_tuple_;
};

template<typename... Iterators>
ZipIterator(Iterators&&...) -> ZipIterator<Iterators...>;

template<typename... Iterators>
void ZipIterator<Iterators...>::increment()
{
  std::apply([](auto&&... it)
  {
    (++it, ...);
  },
  iterator_tuple_);
}

template<typename... Iterators>
void ZipIterator<Iterators...>::decrement()
{
  std::apply([](auto&&... it)
  {
    (--it, ...);
  },
  iterator_tuple_);
}

template<typename... Iterators>
bool ZipIterator<Iterators...>::equal(ZipIterator const& rhs) const
{
    return iterator_tuple_ == rhs.iterator_tuple_;
    //return std::apply([](auto&&...it)
    //{
    //  return ...dont know...;
    //},
    //iterator_tuple_);
}

template<typename... Iterators>
auto ZipIterator<Iterators...>::dereference() const -> reference
{
  return std::apply([](auto&&... it) -> reference
  {
    return {*it...};
  },
  iterator_tuple_);
}

template <typename... Containers>
class Zip
{
public:
  using container_tuple = std::tuple<Containers...>;

  // TODO this could use boost::range_iterator<Containers>::type to get the iterator type.
  // Need to find an edge case first where this is better than simply using Containers::iterator
  // (I guess things like std::array/std::initalisation_list dont have an iterator property)
  using iterator = ZipIterator<typename boost::range_iterator<Containers>::type...>;

  explicit Zip(Containers && ...);

  auto begin() -> iterator;

  auto end() -> iterator;

  auto containers() -> container_tuple;

private:

  container_tuple container_tuple_;
};

template <typename... Containers>
Zip(Containers && ...) -> Zip<Containers...>;

template <typename... Containers>
Zip<Containers...>::Zip(Containers && ... containers) :
  container_tuple_(std::forward<Containers>(containers)...)
{
}

template<typename... Containers>
auto Zip<Containers...>::begin() -> iterator
{
  return std::apply([](auto&&... container) -> iterator
  {
    return {std::begin(container)...};
  },
  container_tuple_);
}

template<typename... Containers>
auto Zip<Containers...>::end() -> iterator
{
  return std::apply([](auto&&... container) -> iterator
  {
    return {std::end(container)...};
  },
  container_tuple_);
}

template <typename... Containers>
auto Zip<Containers...>::containers() -> container_tuple
{
  return container_tuple_;
};

void construct_from_containers_by_value()
{
  std::vector<float> v{1.0f,2.0f,3.0f};
  std::list<int> l{1,2,3};
  Zip z(v, l);

  static_assert(std::is_same<
      decltype(z.containers()),
      std::tuple<
        std::vector<float>&,
        std::list<int>&
      >
    >::value);
}

void construct_from_containers_by_rvalue()
{
  std::vector<float> v{1.0f,2.0f,3.0f};
  std::list<int> l{1,2,3};
  Zip z(std::move(v), std::move(l));

  static_assert(std::is_same<
      decltype(z.containers()),
      std::tuple<
        std::vector<float>,
        std::list<int>
      >
    >::value);
}

void construct_from_containers_with_mix_of_lvalue_and_rvalue()
{
  std::vector<float> v{1.0f,2.0f,3.0f};
  std::list<int> l{1,2,3};
  std::forward_list<int> fl{1,2,3};
  Zip z(
    std::move(v),
    l,
    std::move(fl));

  static_assert(std::is_same<
      decltype(z.containers()),
      std::tuple<
        std::vector<float>,
        std::list<int>&,
        std::forward_list<int>
      >
    >::value);
}

void construct_from_containers_by_value_and_edit_through_tuple_edits_original()
{
  std::vector<int> v{1,2,3};
  std::list<int> l{1,2,3};
  Zip z(v, l);

  std::get<0>(z.containers())[0] = 5;

  if(v[0] != 5) throw std::runtime_error("list not changed");
}

void construct_from_containers_by_rvalue_takes_ownership()
{
  std::vector<int> v{1,2,3};
  std::list<int> l{1,2,3};
  Zip z(std::move(v), std::move(l));

  if(std::get<0>(z.containers()).size() != 3) throw std::runtime_error("Not owned");
  if(std::get<1>(z.containers()).size() != 3) throw std::runtime_error("Not owned");
}

void begin_gives_iterator_with_correct_value_type_for_single_container()
{
  std::vector<int> v{1,2,3};
  Zip z(v);

  static_assert(std::is_same<
    decltype(z.begin())::value_type,
    std::tuple<int>
  >::value);
}

void begin_gives_iterator_with_correct_value_type_for_single_const_container()
{
  std::vector<int> const v{1,2,3};
  Zip z(v);

  static_assert(std::is_same<
    decltype(z.begin())::value_type,
    std::tuple<int>
  >::value);
}

void begin_gives_iterator_with_correct_value_type_for_many_containers()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};
  std::forward_list<bool> fl{true,false,false};
  Zip z(v,l,fl);

  static_assert(std::is_same<
    decltype(z.begin())::value_type,
    std::tuple<int, float, bool>
  >::value);
}

void begin_gives_iterator_with_correct_reference_type_for_many_containers()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};
  std::forward_list<bool> fl{true,false,false};
  Zip z(v,l,fl);

  static_assert(std::is_same<
    decltype(z.begin())::reference,
    std::tuple<int&, float&, bool&>
  >::value);
}

void begin_gives_iterator_with_correct_reference_type_for_many_containers_passed_by_rvalue()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};
  std::forward_list<bool> fl{true,false,false};
  Zip z(
    std::move(v),
    std::move(l),
    std::move(fl));

  static_assert(std::is_same<
    decltype(z.begin())::reference,
    std::tuple<int&, float&, bool&>
  >::value);
}

void begin_gives_iterator_with_correct_category_from_single_container()
{
  std::list<float> l{1.0f,2.0f,3.0f};
  Zip z(l);

  static_assert(std::is_same<
    decltype(z.begin())::iterator_category,
    std::bidirectional_iterator_tag
  >::value);
}

void begin_gives_iterator_with_correct_category_from_many_containers()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};
  std::forward_list<bool> fl{true,false,false};
  Zip z(v,l,fl);

  static_assert(std::is_same<
    decltype(z.begin())::iterator_category,
    std::forward_iterator_tag
  >::value);
}

void construct_iterator_and_get_first_element_has_coorect_type()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};

  auto it = ZipIterator(std::begin(v), std::begin(l));
  static_assert(std::is_same<
    decltype(*it),
    std::tuple<int&, float&>
  >::value);
}

void construct_iterator_and_get_first_element_has_correct_values()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};

  auto it = ZipIterator(std::begin(v), std::begin(l));
  if(*it != std::make_tuple(1, 1.0f))
    throw std::runtime_error("construct_iterator_and_get_first_element_has_correct_values");
}

void construct_iterator_and_get_plusplus_element_has_correct_values()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};

  auto it = ZipIterator(std::begin(l), std::begin(v));
  auto next = ++it;
  if(*next != std::make_tuple(2.0f, 2))
    throw std::runtime_error("construct_iterator_and_get_plusplus_element_has_correct_values");
}

void construct_iterator_and_get_minusminus_element_has_correct_values()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};

  auto it = ZipIterator(std::end(l), std::end(v));
  auto next = --it;
  if(*next != std::make_tuple(3.0f, 3))
    throw std::runtime_error("construct_iterator_and_get_minusminus_element_has_correct_values");
}

void construct_iterator_and_get_next_element_has_correct_values()
{
  std::vector<int> v{1,2,3};
  std::list<float> l{1.0f,2.0f,3.0f};

  auto it = ZipIterator(std::begin(l), std::begin(v));
  auto next = std::next(it);
  if(*next != std::make_tuple(2.0f, 2))
    throw std::runtime_error("construct_iterator_and_get_plusplus_element_has_correct_values");
}

//void get_iterator_to_first_element_of_single_container_and_dereference_gives_correct_value()
//{
//  std::vector<int> v{1,2,3};
//  Zip z(v);
//  auto it = z.begin();
//
//  if(std::get<0>(*it) != 1) throw std::runtime_error("not 1");
//}

void weakest_iterator_type()
{
  WeakestIteratorTag<
    std::bidirectional_iterator_tag,
    std::random_access_iterator_tag>::tag a;

  static_assert(
    std::is_same<decltype(a), std::bidirectional_iterator_tag>::value);
}

void zip_iterator_category()
{
  ZipIteratorTraits<
      std::vector<int>::iterator,
      std::list<int>::iterator,
      std::forward_list<int>::iterator
    >::iterator_category a;

  static_assert(
    std::is_same<decltype(a), std::forward_iterator_tag>::value);
}

void has_size_function()
{
  HasSizeFunction<std::list<int>>::value list;
  HasSizeFunction<int>::value integer;

  if(!list) throw std::runtime_error("list has size");
  if(integer) throw std::runtime_error("integer does not have size");
}

template<typename Container>
class PairedRange
{
public:
  using iterator = ZipIterator<
        typename boost::range_iterator<Container>::type,
        typename boost::range_iterator<Container>::type
        >;

  PairedRange(
      Container&& container) :
    container_(std::forward<Container>(container))
  {
  }

  auto begin() -> iterator
  {
    return {std::begin(container_),
        std::next(std::begin(container_))};
  }

  auto end() -> iterator
  {
    return {
      std::prev(std::end(container_)),
      std::end(container_)};
  }


private:

  Container container_;

};

template<typename Container>
PairedRange(Container&&) -> PairedRange<Container>;

void temp()
{
  std::vector<int> v{1,2,3,4,5,6,7,8};

  for(auto [a, b]: PairedRange{v})
  {
    std::cout << a << " " << b << std::endl;
  }

  int previous;
  for(auto a = std::begin(v);
    a != std::end(v);
    ++a)
  {
    if(a == std::begin(v))
    {
      previous = *a;
    }
    else
    {
      std::cout << previous << " " << *a << std::endl;

    }
  }
}

void use_zip()
{
  std::vector<int> v{1,2,3,4,5};
  std::list<std::string> l{"a", "b", "c", "d", "e"};

  std::list<std::string> other{"fds", "gfds", "hfgd", "jht", "uytr"};

  for(auto [a,b,c] : Zip{v, l, other})
  {
    std::cout << a << " " << b << " " << c << std::endl;
  }

  //auto a = std::begin(v);
  //auto c = std::begin(other);
  //for(auto b : l)
  //{
  //  std::cout << *a << " " << b << " " << *c << std::endl;
  //  ++a;
  //  ++c;
  //}
}

int main()
{
  //construct_from_containers_by_value();
  //construct_from_containers_by_rvalue();
  //construct_from_containers_with_mix_of_lvalue_and_rvalue();
  //construct_from_containers_by_value_and_edit_through_tuple_edits_original();
  //construct_from_containers_by_rvalue_takes_ownership();
  //begin_gives_iterator_with_correct_value_type_for_single_container();
  //begin_gives_iterator_with_correct_value_type_for_single_const_container();
  //begin_gives_iterator_with_correct_category_from_single_container();
  //begin_gives_iterator_with_correct_category_from_many_containers();
  ////get_iterator_to_first_element_of_single_container_and_dereference_gives_correct_value();

  //construct_iterator_and_get_first_element_has_coorect_type();
  //construct_iterator_and_get_plusplus_element_has_correct_values();
  //construct_iterator_and_get_minusminus_element_has_correct_values();
  //construct_iterator_and_get_next_element_has_correct_values();


  //weakest_iterator_type();
  //zip_iterator_category();
  //has_size_function();



  // Demos....
  temp();

  //use_zip();
}
