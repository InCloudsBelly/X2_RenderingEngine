#pragma once
#include <typeinfo>
#include <string>
#include <rttr/type>
#include <rttr/registration>

#define CONSTRUCTOR(T) \
	T();\
	virtual ~T();\
	T(const T&) = delete;\
	T& operator=(const T&) = delete;\
	T(T&&) = delete;\
	T& operator=(T&&) = delete;

class Object
{
public:
	Object();
	virtual ~Object();
	rttr::type type();
	virtual std::string toString();
protected:
	virtual void onAwake();
	RTTR_ENABLE()
};