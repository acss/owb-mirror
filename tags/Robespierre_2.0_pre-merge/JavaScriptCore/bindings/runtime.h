/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JAVASCRIPTCORE_BINDINGS_RUNTIME_H
#define JAVASCRIPTCORE_BINDINGS_RUNTIME_H

#include "value.h"

#include <wtf/HashMap.h>

namespace KJS  {

class Identifier;
class List;

namespace Bindings {

class Instance;
class Method;
class RootObject;

// For now just use Java style type descriptors.
typedef const char * RuntimeType;

// FIXME:  Parameter should be removed from abstract runtime classes.
class Parameter
{
public:
    virtual RuntimeType type() const = 0;
    virtual ~Parameter() {}
};

// FIXME:  Constructor should be removed from abstract runtime classes
// unless we want to support instantiation of runtime objects from
// JavaScript.
class Constructor
{
public:
    virtual Parameter* parameterAt(int i) const = 0;
    virtual int numParameters() const = 0;

    virtual ~Constructor() {}
};

class Field
{
public:
    virtual const char* name() const = 0;
    virtual RuntimeType type() const = 0;

    virtual JSValue* valueFromInstance(ExecState*, const Instance*) const = 0;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue*) const = 0;

    virtual ~Field() {}
};

class MethodList
{
public:
    MethodList() : _methods(0), _length(0) {}
    
    void addMethod(Method*);
    unsigned int length() const;
    Method* methodAt(unsigned int index) const;
    
    ~MethodList();
    
    MethodList(const MethodList&);
    MethodList& operator=(const MethodList&);

private:
    Method **_methods;
    unsigned int _length;
};

class Method
{
public:
    virtual const char *name() const = 0;

    virtual int numParameters() const = 0;
        
    virtual ~Method() {}
};

class Class
{
public:
    virtual const char *name() const = 0;
    
    virtual MethodList methodsNamed(const Identifier&, Instance*) const = 0;
    
    virtual Constructor *constructorAt(int i) const = 0;
    virtual int numConstructors() const = 0;
    
    virtual Field *fieldNamed(const Identifier&, Instance*) const = 0;

    virtual JSValue* fallbackObject(ExecState*, Instance*, const Identifier&) { return jsUndefined(); }
    
    virtual ~Class() {}
};

typedef void (*KJSDidExecuteFunctionPtr)(ExecState*, JSObject* rootObject);

class Instance
{
public:
    typedef enum {
        JavaLanguage,
        ObjectiveCLanguage,
        CLanguage
#if PLATFORM(QT)
        , QtLanguage
#endif
#ifdef __OWB__
	,BalLanguage
#endif
    } BindingLanguage;

    Instance();

    static void setDidExecuteFunction(KJSDidExecuteFunctionPtr func);
    static KJSDidExecuteFunctionPtr didExecuteFunction();
    
    static Instance* createBindingForLanguageInstance(BindingLanguage, void* nativeInstance, PassRefPtr<RootObject> = 0);
    static JSObject* createRuntimeObject(BindingLanguage, void* nativeInstance, PassRefPtr<RootObject> = 0);

    void ref() { _refCount++; }
    void deref() 
    { 
        if (--_refCount == 0) 
            delete this; 
    }

    // These functions are called before and after the main entry points into
    // the native implementations.  They can be used to establish and cleanup
    // any needed state.
    virtual void begin() {}
    virtual void end() {}
    
    virtual Class *getClass() const = 0;
    
    virtual JSValue* getValueOfField(ExecState*, const Field*) const;
    virtual JSValue* getValueOfUndefinedField(ExecState*, const Identifier&, JSType) const { return jsUndefined(); }
    virtual void setValueOfField(ExecState*, const Field*, JSValue*) const;
    virtual bool supportsSetValueOfUndefinedField() { return false; }
    virtual void setValueOfUndefinedField(ExecState*, const Identifier&, JSValue*) {}

    virtual bool implementsCall() const { return false; }
    
    virtual JSValue* invokeMethod(ExecState*, const MethodList&, const List& args) = 0;
    virtual JSValue* invokeDefaultMethod(ExecState*, const List&) { return jsUndefined(); }
    
    virtual JSValue* defaultValue(JSType hint) const = 0;
    
    virtual JSValue* valueOf() const { return jsString(getClass()->name()); }
    
    void setRootObject(PassRefPtr<RootObject>);
    RootObject* rootObject() const;
    
    virtual ~Instance();

protected:
    RefPtr<RootObject> _rootObject;
    unsigned _refCount;

private:
    Instance(const Instance &other); // prevent copying
    Instance &operator=(const Instance &other); // ditto
};

class Array
{
public:
    virtual void setValueAt(ExecState *, unsigned index, JSValue*) const = 0;
    virtual JSValue* valueAt(ExecState *, unsigned index) const = 0;
    virtual unsigned int getLength() const = 0;
    virtual ~Array() {}
};

const char *signatureForParameters(const List&);

typedef HashMap<RefPtr<UString::Rep>, MethodList*> MethodListMap;
typedef HashMap<RefPtr<UString::Rep>, Method*> MethodMap; 
typedef HashMap<RefPtr<UString::Rep>, Field*> FieldMap; 
    
} // namespace Bindings

} // namespace KJS

#endif
