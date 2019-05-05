//
// Created by dguco on 19-3-23.
//

#include <iostream>
#include <functional>
#include <utility>      // std::declval
#include <iostream>     // std::cout
#include <memory>
#include <map>
#include <vector>

struct A
{              // abstract class
    virtual int value() = 0;
};

struct C
{
    int a;
};

class B
{    // class with specific constructor
    int val_;
public:
    B()
    {
        val_ = 0;
        printf("B default construct\n");
    }
    B(int i, int j)
        : val_(i * j)
    {
        printf("B construct,i = %d,j= %d\n", i, j);
    }
    B(const B &other)
    {
        printf("B copy construct\n");
        this->val_ = other.val_;
    }
    B(const B &&other)
    {
        printf("B move construct\n");
        this->val_ = other.val_;
    }
    B &operator=(const B &b)
    {
        this->val_ = b.val_;
        printf("B operator=\n");
        return *this;
    };

    int value()
    { return val_; }
};

void test()
{
    decltype(std::declval<A>().value()) a;  // int a
    decltype(std::declval<B>().value()) b;  // int b
    decltype(B(0, 0).value()) c;   // same as above (known constructor)
    a = b = B(10, 2).value();
    typedef decltype(std::declval<C>()) type_c;
    C *ccc = new C;
    type_c __c = std::move(*ccc);
    std::cout << a << '\n';

    std::shared_ptr<int> ip1 = std::make_shared<int>(10);
    int use = ip1.use_count();
    std::shared_ptr<int> ip2 = ip1;
    use = ip1.use_count();
    use = ip2.use_count();
    std::shared_ptr<int> ip3 = std::move(ip2);
    use = ip1.use_count();
    use = ip2.use_count();
    use = ip3.use_count();
    ip3.reset();
    if (ip3) {
        std::cout << "Ok" << std::endl;
    }
    else {
        std::cout << "failed" << std::endl;
    }
};

#define _OFFSET_(_Obj_Ty,_Key)                                                    \
    ((unsigned long)(&((_Obj_Ty *)0)->_Key))

#define CLASS_REGISTER(_Obj_Ty)                                                    \
public:                                                                            \
    static tat_class * get_class_ptr()                                        \
    {                                                                            \
        static tat_class __class_##_Obj_Key##__;                            \
        return &__class_##_Obj_Key##__;                                            \
    }

#define FIELD_REGISTER(_Access,_Field_Ty,_Field_Key,_Obj_Ty)                    \
_Access:                                                                        \
    _Field_Ty _Field_Key;                                                        \
private:                                                                        \
    class __field_register_##_Field_Key##__                                        \
    {                                                                            \
    public:                                                                        \
        __field_register_##_Field_Key##__()                                        \
        {                                                                        \
            static __field_register__ reg_##_Field_Key(                    \
                _Obj_Ty::get_class_ptr(),                                        \
                _OFFSET_(_Obj_Ty,_Field_Key),                                    \
                #_Field_Key);                                                    \
        }                                                                        \
    }_Field_Key##_register;

class tat_field
{
private:
    unsigned long _offset;
    std::string _key;
public:
    tat_field(unsigned long offset, std::string key) :_offset(offset), _key(key) {}
    tat_field(const tat_field &field)
    {
        this->_offset = field._offset;
        this->_key = field._key;
    }
public:
    template<typename _Obj_Ty, typename _Value_Ty>
    void get(_Obj_Ty *obj, _Value_Ty &value)
    {
        value = *((_Value_Ty *)((unsigned char *)obj + _offset));
    }
    template<typename _Obj_Ty, typename _Value_Ty>
    void set(_Obj_Ty *obj, const _Value_Ty &value)
    {
        *((_Value_Ty *)((unsigned char *)obj + _offset)) = value;
    }
    std::string get_key() const
    {
        return this->_key;
    }
};

class tat_class
{
private:
    std::map<std::string, tat_field> _field_map;
    std::string _key;
public:
    std::map<std::string, tat_field> get_fields()
    {
        return this->_field_map;
    }
    tat_field get_field(std::string key)
    {
        std::map<std::string, tat_field>::iterator itr = _field_map.find(key);
        return (*itr).second;
    }
    void add_field(const tat_field &field)
    {
        _field_map.insert(std::pair<std::string, tat_field>(field.get_key(), field));
    }
};

class __field_register__
{
public:
    __field_register__(tat_class *class_ptr, unsigned long offset, std::string key)
    {
        tat_field field(offset, key);
        class_ptr->add_field(field);
    }
};

class TestClass
{
public:
    TestClass() = default;
    ~TestClass() = default;

CLASS_REGISTER(TestClass)
FIELD_REGISTER(public, long, _long_f, TestClass)
FIELD_REGISTER(public, int, _int_f, TestClass)
FIELD_REGISTER(public, std::string, _str_f, TestClass)
FIELD_REGISTER(public, std::vector<int>, _vec_f, TestClass)
};

void testReflection()
{
    TestClass inst;

    tat_class *test_class = TestClass::get_class_ptr();
    std::map<std::string, tat_field> field_map = test_class->get_fields();


    for (auto& var : field_map)
    {
        std::cout << var.first << std::endl;
    }

    tat_field test_vec_field = field_map.find("_vec_f")->second;
    std::vector<int> vec;
    test_vec_field.get(&inst, vec);
    vec.push_back(22);
    test_vec_field.set(&inst, vec);
    std::cout << inst._vec_f[0] << std::endl;
}
int main()
{
    int a_ = 0;
    B bbb = [&a_]() -> B
    {
        printf("Call func a_ = %d\n", a_);
    }();
    printf("bbb value = %d \n", bbb.value());
    testReflection();
    return 0;
}