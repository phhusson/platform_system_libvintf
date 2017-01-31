/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Convert objects from and to xml.

#include <tinyxml2.h>

#include "parse_string.h"
#include "parse_xml.h"

namespace android {
namespace vintf {

// --------------- tinyxml2 details

using NodeType = tinyxml2::XMLElement;
using DocType = tinyxml2::XMLDocument;

// caller is responsible for deleteDocument() call
inline DocType *createDocument() {
    return new tinyxml2::XMLDocument();
}

// caller is responsible for deleteDocument() call
inline DocType *createDocument(const std::string &xml) {
    DocType *doc = new tinyxml2::XMLDocument();
    if (doc->Parse(xml.c_str()) == tinyxml2::XML_NO_ERROR) {
        return doc;
    }
    delete doc;
    return nullptr;
}

inline void deleteDocument(DocType *d) {
    delete d;
}

inline std::string printDocument(DocType *d) {
    tinyxml2::XMLPrinter p;
    d->Print(&p);
    return std::string{p.CStr()};
}

inline NodeType *createNode(const std::string &name, DocType *d) {
    return d->NewElement(name.c_str());
}

inline void appendChild(NodeType *parent, NodeType *child) {
    parent->InsertEndChild(child);
}

inline void appendChild(DocType *parent, NodeType *child) {
    parent->InsertEndChild(child);
}

inline void appendStrAttr(NodeType *e, const std::string &attrName, const std::string &attr) {
    e->SetAttribute(attrName.c_str(), attr.c_str());
}

// text -> text
inline void appendText(NodeType *parent, const std::string &text, DocType *d) {
    parent->InsertEndChild(d->NewText(text.c_str()));
}

// text -> <name>text</name>
inline void appendTextElement(NodeType *parent, const std::string &name,
            const std::string &text, DocType *d) {
    NodeType *c = createNode(name, d);
    appendText(c, text, d);
    appendChild(parent, c);
}

inline std::string nameOf(NodeType *root) {
    return root->Name() == NULL ? "" : root->Name();
}

inline std::string getText(NodeType *root) {
    return root->GetText() == NULL ? "" : root->GetText();
}

inline NodeType *getChild(NodeType *parent, const std::string &name) {
    return parent->FirstChildElement(name.c_str());
}

inline NodeType *getRootChild(DocType *parent) {
    return parent->FirstChildElement();
}

inline std::vector<NodeType *> getChildren(NodeType *parent, const std::string &name) {
    std::vector<NodeType *> v;
    for (NodeType *child = parent->FirstChildElement(name.c_str());
         child != nullptr;
         child = child->NextSiblingElement(name.c_str())) {
        v.push_back(child);
    }
    return v;
}

inline bool getAttr(NodeType *root, const std::string &attrName, std::string *s) {
    const char *c = root->Attribute(attrName.c_str());
    if (c == NULL)
        return false;
    *s = c;
    return true;
}

// --------------- tinyxml2 details end.

// ---------------------- XmlNodeConverter definitions

template<typename Object>
struct XmlNodeConverter : public XmlConverter<Object> {
    XmlNodeConverter() {}
    virtual ~XmlNodeConverter() {}

    // sub-types should implement these.
    virtual void mutateNode(const Object &o, NodeType *n, DocType *d) const = 0;
    virtual bool buildObject(Object *o, NodeType *n) const = 0;
    virtual std::string elementName() const = 0;

    // convenience methods for user
    inline const std::string &lastError() const { return mLastError; }
    inline NodeType *serialize(const Object &o, DocType *d) const {
        NodeType *root = createNode(this->elementName(), d);
        this->mutateNode(o, root, d);
        return root;
    }
    inline std::string serialize(const Object &o) const {
        DocType *doc = createDocument();
        appendChild(doc, serialize(o, doc));
        std::string s = printDocument(doc);
        deleteDocument(doc);
        return s;
    }
    inline bool deserialize(Object *object, NodeType *root) const {
        if (nameOf(root) != this->elementName()) {
            return false;
        }
        return this->buildObject(object, root);
    }
    inline bool deserialize(Object *o, const std::string &xml) const {
        DocType *doc = createDocument(xml);
        bool ret = doc != nullptr
            && deserialize(o, getRootChild(doc));
        deleteDocument(doc);
        return ret;
    }
    inline NodeType *operator()(const Object &o, DocType *d) const {
        return serialize(o, d);
    }
    inline std::string operator()(const Object &o) const {
        return serialize(o);
    }
    inline bool operator()(Object *o, NodeType *node) const {
        return deserialize(o, node);
    }
    inline bool operator()(Object *o, const std::string &xml) const {
        return deserialize(o, xml);
    }

    // convenience methods for implementor.
    template <typename T>
    inline void appendAttr(NodeType *e, const std::string &attrName, const T &attr) const {
        return appendStrAttr(e, attrName, ::android::vintf::to_string(attr));
    }

    inline void appendAttr(NodeType *e, const std::string &attrName, bool attr) const {
        return appendStrAttr(e, attrName, attr ? "true" : "false");
    }

    template <typename T>
    inline bool parseAttr(NodeType *root, const std::string &attrName, T *attr) const {
        std::string attrText;
        bool ret = getAttr(root, attrName, &attrText) && ::android::vintf::parse(attrText, attr);
        if (!ret) {
            mLastError = "Could not parse attr with name " + attrName;
        }
        return ret;
    }

    inline bool parseAttr(NodeType *root, const std::string &attrName, std::string *attr) const {
        bool ret = getAttr(root, attrName, attr);
        if (!ret) {
            mLastError = "Could not find attr with name " + attrName;
        }
        return ret;
    }

    inline bool parseAttr(NodeType *root, const std::string &attrName, bool *attr) const {
        std::string attrText;
        if (!getAttr(root, attrName, &attrText)) {
            mLastError = "Could not find attr with name " + attrName;
            return false;
        }
        if (attrText == "true" || attrText == "1") {
            *attr = true;
            return true;
        }
        if (attrText == "false" || attrText == "0") {
            *attr = false;
            return true;
        }
        mLastError = "Could not parse attr with name \"" + attrName
                + "\" and value \"" + attrText + "\"";
        return false;
    }

    inline bool parseTextElement(NodeType *root,
            const std::string &elementName, std::string *s) const {
        NodeType *child = getChild(root, elementName);
        if (child == nullptr) {
            mLastError = "Could not find element with name " + elementName;
            return false;
        }
        *s = getText(child);
        return true;
    }

    template <typename T>
    inline bool parseChild(NodeType *root, const XmlNodeConverter<T> &conv, T *t) const {
        NodeType *child = getChild(root, conv.elementName());
        if (child == nullptr) {
            mLastError = "Could not find element with name " + conv.elementName();
            return false;
        }
        bool success = conv.deserialize(t, child);
        if (!success) {
            mLastError = conv.lastError();
        }
        return success;
    }

    template <typename T>
    inline bool parseChildren(NodeType *root, const XmlNodeConverter<T> &conv, std::vector<T> *v) const {
        auto nodes = getChildren(root, conv.elementName());
        v->resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!conv.deserialize(&v->at(i), nodes[i])) {
                mLastError = "Could not parse element with name " + conv.elementName();
                return false;
            }
        }
        return true;
    }

    inline bool parseText(NodeType *node, std::string *s) const {
        *s = getText(node);
        return true;
    }

protected:
    mutable std::string mLastError;
};

template<typename Object>
struct XmlTextConverter : public XmlNodeConverter<Object> {
    XmlTextConverter(const std::string &elementName)
        : mElementName(elementName) {}

    virtual void mutateNode(const Object &object, NodeType *root, DocType *d) const override {
        appendText(root, ::android::vintf::to_string(object), d);
    }
    virtual bool buildObject(Object *object, NodeType *root) const override {
        std::string text = getText(root);
        bool ret = ::android::vintf::parse(text, object);
        if (!ret) {
            this->mLastError = "Could not parse " + text;
        }
        return ret;
    }
    virtual std::string elementName() const { return mElementName; };
private:
    std::string mElementName;
};

// ---------------------- XmlNodeConverter definitions end

const XmlTextConverter<Version> versionConverter{"version"};
const XmlConverter<Version> &gVersionConverter = versionConverter;

const XmlTextConverter<VersionRange> versionRangeConverter{"version"};
const XmlConverter<VersionRange> &gVersionRangeConverter = versionRangeConverter;

const XmlTextConverter<KernelConfig> kernelConfigConverter{"config"};
const XmlConverter<KernelConfig> &gKernelConfigConverter = kernelConfigConverter;

const XmlTextConverter<Transport> transportConverter{"transport"};
const XmlConverter<Transport> &gTransportConverter = transportConverter;

struct MatrixHalConverter : public XmlNodeConverter<MatrixHal> {
    std::string elementName() const override { return "hal"; }
    void mutateNode(const MatrixHal &hal, NodeType *root, DocType *d) const override {
        appendAttr(root, "format", hal.format);
        appendAttr(root, "optional", hal.optional);
        appendTextElement(root, "name", hal.name, d);
        for (const auto &version : hal.versionRanges) {
            appendChild(root, versionRangeConverter(version, d));
        }
    }
    bool buildObject(MatrixHal *object, NodeType *root) const override {
        if (   !parseAttr(root, "format", &object->format)
            || !parseAttr(root, "optional", &object->optional)
            || !parseTextElement(root, "name", &object->name)
            || !parseChildren(root, versionRangeConverter, &object->versionRanges)) {
            return false;
        }
        return true;
    }
};

const MatrixHalConverter matrixHalConverter{};
const XmlConverter<MatrixHal> &gMatrixHalConverter = matrixHalConverter;

struct MatrixKernelConverter : public XmlNodeConverter<MatrixKernel> {
    std::string elementName() const override { return "kernel"; }
    void mutateNode(const MatrixKernel &kernel, NodeType *root, DocType *d) const override {
        appendAttr(root, "version", kernel.version);
        for (const auto &config : kernel.configs) {
            appendChild(root, kernelConfigConverter(config, d));
        }
    }
    bool buildObject(MatrixKernel *object, NodeType *root) const override {
        if (   !parseAttr(root, "version", &object->version)
            || !parseChildren(root, kernelConfigConverter, &object->configs)) {
            return false;
        }
        return true;
    }
};

const MatrixKernelConverter matrixKernelConverter{};
const XmlConverter<MatrixKernel> &gMatrixKernelConverter = matrixKernelConverter;

struct HalImplementationConverter : public XmlNodeConverter<HalImplementation> {
    std::string elementName() const override { return "impl"; }
    void mutateNode(const HalImplementation &impl, NodeType *root, DocType *d) const override {
        appendAttr(root, "level", impl.implLevel);
        appendText(root, impl.impl, d);
    }
    bool buildObject(HalImplementation *object, NodeType *root) const override {
        if (   !parseAttr(root, "level", &object->implLevel)
            || !parseText(root, &object->impl)) {
            return false;
        }
        return true;
    }
};

const HalImplementationConverter halImplementationConverter{};
const XmlConverter<HalImplementation> &gHalImplementationConverter = halImplementationConverter;

struct ManifestHalConverter : public XmlNodeConverter<ManifestHal> {
    std::string elementName() const override { return "hal"; }
    void mutateNode(const ManifestHal &hal, NodeType *root, DocType *d) const override {
        appendAttr(root, "format", hal.format);
        appendTextElement(root, "name", hal.name, d);
        appendChild(root, transportConverter(hal.transport, d));
        appendChild(root,
            halImplementationConverter(hal.impl, d));
        for (const auto &version : hal.versions) {
            appendChild(root, versionConverter(version, d));
        }
    }
    bool buildObject(ManifestHal *object, NodeType *root) const override {
        if (   !parseAttr(root, "format", &object->format)
            || !parseTextElement(root, "name", &object->name)
            || !parseChild(root, transportConverter, &object->transport)
            || !parseChild(root, halImplementationConverter, &object->impl)
            || !parseChildren(root, versionConverter, &object->versions)) {
            return false;
        }
        return true;
    }
};

const ManifestHalConverter manifestHalConverter{};
const XmlConverter<ManifestHal> &gManifestHalConverter = manifestHalConverter;

struct SepolicyConverter : public XmlNodeConverter<Sepolicy> {
    std::string elementName() const override { return "sepolicy"; }
    void mutateNode(const Sepolicy &, NodeType *, DocType *) const override {
        // TODO after writing sepolicy
    }
    bool buildObject(Sepolicy *, NodeType *) const override {
        // TODO after writing sepolicy
        return true;
    }
};
const SepolicyConverter sepolicyConverter{};
const XmlConverter<Sepolicy> &gSepolicyConverter = sepolicyConverter;

struct VendorManifestConverter : public XmlNodeConverter<VendorManifest> {
    std::string elementName() const override { return "manifest"; }
    void mutateNode(const VendorManifest &m, NodeType *root, DocType *d) const override {
        appendAttr(root, "version", VendorManifest::kVersion);
        for (const auto &hal : m.getHals()) {
            appendChild(root, manifestHalConverter(hal, d));
        }
    }
    bool buildObject(VendorManifest *object, NodeType *root) const override {
        std::vector<ManifestHal> hals;
        if (!parseChildren(root, manifestHalConverter, &hals)) {
            return false;
        }
        for (auto &&hal : hals) {
            object->add(std::move(hal));
        }
        return true;
    }
};

const VendorManifestConverter vendorManifestConverter{};
const XmlConverter<VendorManifest> &gVendorManifestConverter = vendorManifestConverter;

struct CompatibilityMatrixConverter : public XmlNodeConverter<CompatibilityMatrix> {
    std::string elementName() const override { return "compatibility-matrix"; }
    void mutateNode(const CompatibilityMatrix &m, NodeType *root, DocType *d) const override {
        appendAttr(root, "version", CompatibilityMatrix::kVersion);
        for (const auto &pair : m.hals) {
            appendChild(root, matrixHalConverter(pair.second, d));
        }
        for (const auto &kernel : m.kernels) {
            appendChild(root, matrixKernelConverter(kernel, d));
        }
        appendChild(root, sepolicyConverter(m.sepolicy, d));
    }
    bool buildObject(CompatibilityMatrix *object, NodeType *root) const override {
        std::vector<MatrixHal> hals;
        if (   !parseChildren(root, matrixHalConverter, &hals)
            || !parseChildren(root, matrixKernelConverter, &object->kernels)
            || !parseChild(root, sepolicyConverter, &object->sepolicy)) {
            return false;
        }
        for (auto &&hal : hals) {
            object->add(std::move(hal));
        }
        return true;
    }
};

const CompatibilityMatrixConverter compatibilityMatrixConverter{};
const XmlConverter<CompatibilityMatrix> &gCompatibilityMatrixConverter
        = compatibilityMatrixConverter;

} // namespace vintf
} // namespace android
