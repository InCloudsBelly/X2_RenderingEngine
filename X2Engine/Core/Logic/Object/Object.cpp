#include "Object.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
	rttr::registration::class_<Object>("Object");
}

Object::Object()
{
}

Object::~Object()
{
}

rttr::type Object::type()
{
	return rttr::type::get(*this);
}

std::string Object::toString()
{
	return type().get_name().to_string();
}


void Object::onAwake()
{
}
