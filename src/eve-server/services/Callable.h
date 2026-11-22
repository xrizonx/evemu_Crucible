//
// Created by games on 15/01/2023.
//

#ifndef EVEMU_CALLABLE_H
#define EVEMU_CALLABLE_H

#include <functional>
#include <map>
#include <optional>

#include "eve-server.h"

class Client;
class PyResult;

enum AccessLevel {
    eAccessLevel_None = 0,
    eAccessLevel_Location = 1,
    eAccessLevel_LocationPreferred = 2,
    eAccessLevel_SolarSystem = 3,
    eAccessLevel_SolarSystem2 = 4,
    eAccessLevel_Station = 5,
    eAccessLevel_Character = 6,
    eAccessLevel_Corporation = 7,
    eAccessLevel_User = 8
};

class method_not_found {
public:
    method_not_found() {}
};

class PyCallArgs
{
public:
    PyCallArgs(Client* c, PyTuple* tup, PyDict* dict);
    ~PyCallArgs();

    void Dump(LogType type) const;

    // Optional completion polled by the owning client on the game thread.
    using DeferredReply = std::function<bool(PyResult&)>;
    bool allowDeferredReply = false;
    DeferredReply deferredReply;

    Client* const client;    //we do not own this
    PyTuple* tuple;        //we own this, but it may be taken
    std::map<std::string, PyRep*> byname;    //we own this, but elements may be taken.
};

class PyResult
{
public:
    PyResult();
    PyResult(PyRep* result);
    PyResult(PyRep* result, PyDict* namedResult);
    PyResult(const PyResult& oth);
    ~PyResult();

    PyResult& operator=(const PyResult& oth);

    PyRep* ssResult;
    PyDict* ssNamedResult;
};


template <class T> struct is_optional : std::false_type {};
template <class T> struct is_optional<std::optional<T>> : std::true_type {};

struct CallHandlerBase {
public:
    virtual ~CallHandlerBase() = default;
    virtual PyResult operator()(PyCallArgs& args) const = 0;
    virtual const std::string& getSignature() = 0;
};

template <class S>
struct CallHandler : public CallHandlerBase {
    // Bind the service instance and its exact member-function signature
    // at registration time. Erasing a member pointer as a different member
    // signature is undefined behavior (especially under multiple inheritance).
    template<class... Args>
    CallHandler(S* service, PyResult(S::*callHandler)(PyCallArgs&, Args...)) :
        handlerImpl {
            [this, service, callHandler](PyCallArgs& args) -> PyResult {
                if constexpr (sizeof...(Args) == 0) {
                    if (args.tuple->size() != 0)
                        throw std::invalid_argument("expected 0 arguments");
                    return (service->*callHandler)(args);
                } else {
                    if (!this->validateArgs<std::decay_t<Args>...>(args))
                        throw std::invalid_argument("arguments do not match");
                    return this->apply(service, callHandler, args);
                }
            }
        }
    {
        this->generateSignature<std::decay_t<Args>...>();
    }

    PyResult operator()(PyCallArgs& args) const override {
        return handlerImpl(args);
    }

    const std::string& getSignature() override {
        return this->signature;
    }

private:
    template <typename T> bool validateArg(size_t index, PyCallArgs& args) {
        // handle optional values
        if constexpr (is_optional <T>::value) {
            // treat Nones on optional values as valid
            if (index >= args.tuple->size() || args.tuple->GetItem(index)->IsNone()) {
                return true;
            } else {
                return validateArg <typename T::value_type>(index, args);
            }
        }

        if (index >= args.tuple->size()) {
            return false;
        }

        if constexpr (std::is_same_v <T, PyRep*>)
            return true;

        PyRep* rep = args.tuple->GetItem(index);

        // validate type with their parameter equivalent
        if constexpr (std::is_same_v <T, PyBool*>)
            return rep->IsBool();
        if constexpr (std::is_same_v <T, PyInt*>)
            return rep->IsInt();
        if constexpr (std::is_same_v <T, PyLong*>)
            return rep->IsLong();
        if constexpr (std::is_same_v <T, PyFloat*>)
            return rep->IsFloat();
        if constexpr (std::is_same_v <T, PyBuffer*>)
            return rep->IsBuffer();
        if constexpr (std::is_same_v <T, PyString*>)
            return rep->IsString();
        if constexpr (std::is_same_v <T, PyWString*>)
            return rep->IsWString();
        if constexpr (std::is_same_v <T, PyToken*>)
            return rep->IsToken();
        if constexpr (std::is_same_v <T, PyTuple*>)
            return rep->IsTuple();
        if constexpr (std::is_same_v <T, PyList*>)
            return rep->IsList();
        if constexpr (std::is_same_v <T, PyDict*>)
            return rep->IsDict();
        if constexpr (std::is_same_v <T, PyNone*>)
            return rep->IsNone();
        if constexpr (std::is_same_v <T, PySubStruct*>)
            return rep->IsSubStruct();
        if constexpr (std::is_same_v <T, PySubStream*>)
            return rep->IsSubStream();
        if constexpr (std::is_same_v <T, PyChecksumedStream*>)
            return rep->IsChecksumedStream();
        if constexpr (std::is_same_v <T, PyObject*>)
            return rep->IsObject();
        if constexpr (std::is_same_v <T, PyObjectEx*>)
            return rep->IsObjectEx();
        if constexpr (std::is_same_v <T, PyPackedRow*>)
            return rep->IsPackedRow();

        return false;
    }

    template <typename T> decltype(auto) getAs(size_t index, PyTuple* tup) {
        if constexpr (is_optional <T>::value) {
            if (index >= tup->size() || tup->GetItem(index)->IsNone()) {
                return T{};
            } else {
                return std::make_optional(getAs <typename T::value_type>(index, tup));
            }
        }

        if (index >= tup->size()) {
            throw std::runtime_error("This should not happen. Trying to get parameter out of bounds. What happened to the validation?!");
        }

        PyRep* rep = tup->GetItem(index);

        if constexpr (std::is_same_v <T, PyRep*>)
            return rep;
        if constexpr (std::is_same_v <T, PyBool*>)
            return rep->AsBool();
        if constexpr (std::is_same_v <T, PyInt*>)
            return rep->AsInt();
        if constexpr (std::is_same_v <T, PyLong*>)
            return rep->AsLong();
        if constexpr (std::is_same_v <T, PyFloat*>)
            return rep->AsFloat();
        if constexpr (std::is_same_v <T, PyBuffer*>)
            return rep->AsBuffer();
        if constexpr (std::is_same_v <T, PyString*>)
            return rep->AsString();
        if constexpr (std::is_same_v <T, PyWString*>)
            return rep->AsWString();
        if constexpr (std::is_same_v <T, PyToken*>)
            return rep->AsToken();
        if constexpr (std::is_same_v <T, PyTuple*>)
            return rep->AsTuple();
        if constexpr (std::is_same_v <T, PyList*>)
            return rep->AsList();
        if constexpr (std::is_same_v <T, PyDict*>)
            return rep->AsDict();
        if constexpr (std::is_same_v <T, PyNone*>)
            return rep->AsNone();
        if constexpr (std::is_same_v <T, PySubStruct*>)
            return rep->AsSubStruct();
        if constexpr (std::is_same_v <T, PySubStream*>)
            return rep->AsSubStream();
        if constexpr (std::is_same_v <T, PyChecksumedStream*>)
            return rep->AsChecksumedStream();
        if constexpr (std::is_same_v <T, PyObject*>)
            return rep->AsObject();
        if constexpr (std::is_same_v <T, PyObjectEx*>)
            return rep->AsObjectEx();
        if constexpr (std::is_same_v <T, PyPackedRow*>)
            return rep->AsPackedRow();
    }

    template<class... Args>
    bool validateArgs(PyCallArgs& args) {
        constexpr size_t ArgCount = sizeof... (Args);
        const bool argCountIsValid = args.tuple->size() <= ArgCount;

        if (argCountIsValid == false) {
            return false;
        }

        bool isValid = true;
        size_t index = 0;

        ((
            isValid = validateArg<Args>(index, args) && isValid, ++index
        ), ...);

        return isValid;
    }

    template <typename T> std::string translateParameter() {
        // handle optional values
        if constexpr (is_optional <T>::value) {
            // treat Nones on optional values as valid
            return "std::optional<" + translateParameter <typename T::value_type>() + ">";
        }

        if constexpr (std::is_same_v <T, PyRep*>)
            return "PyRep*";

        // validate type with their parameter equivalent
        if constexpr (std::is_same_v <T, PyBool*>)
            return "PyBool*";
        if constexpr (std::is_same_v <T, PyInt*>)
            return "PyInt*";
        if constexpr (std::is_same_v <T, PyLong*>)
            return "PyLong*";
        if constexpr (std::is_same_v <T, PyFloat*>)
            return "PyFloat*";
        if constexpr (std::is_same_v <T, PyBuffer*>)
            return "PyBuffer*";
        if constexpr (std::is_same_v <T, PyString*>)
            return "PyString*";
        if constexpr (std::is_same_v <T, PyWString*>)
            return "PyWString*";
        if constexpr (std::is_same_v <T, PyToken*>)
            return "PyToken*";
        if constexpr (std::is_same_v <T, PyTuple*>)
            return "PyTuple*";
        if constexpr (std::is_same_v <T, PyList*>)
            return "PyList*";
        if constexpr (std::is_same_v <T, PyDict*>)
            return "PyDict*";
        if constexpr (std::is_same_v <T, PyNone*>)
            return "PyNone*";
        if constexpr (std::is_same_v <T, PySubStruct*>)
            return "PySubStruct*";
        if constexpr (std::is_same_v <T, PySubStream*>)
            return "PySubStream*";
        if constexpr (std::is_same_v <T, PyChecksumedStream*>)
            return "PyChecksumedStream*";
        if constexpr (std::is_same_v <T, PyObject*>)
            return "PyObject*";
        if constexpr (std::is_same_v <T, PyObjectEx*>)
            return "PyObjectEx*";
        if constexpr (std::is_same_v <T, PyPackedRow*>)
            return "PyPackedRow*";

        return "Unknown";
    }

    template<class... Args>
    void generateSignature () {
        constexpr size_t length = sizeof... (Args);
        size_t index = 0;

        ((
            signature += translateParameter <Args> (), signature += (index < (length - 1) ? "," : ""), ++ index
        ), ...);
    }

    template<class... Args, size_t... I>
    PyResult applyImpl(
            S* service,
            PyResult(S::*handler)(PyCallArgs& args, Args...),
            PyCallArgs& args,
            std::index_sequence<I...>) {
        return (service->*handler) (args, getAs<std::decay_t<Args>>(I, args.tuple)...);
    }

    template<class... Args>
    PyResult apply(
            S* service,
            PyResult(S::*handler) (PyCallArgs& args, Args...),
            PyCallArgs& args) {
        return applyImpl(
                service, handler, args,
                std::make_index_sequence <sizeof...(Args)>{}
        );
    }

    std::function<PyResult(PyCallArgs&)> handlerImpl;
    std::string signature;
};

#endif //EVEMU_CALLABLE_H
