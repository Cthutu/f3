//----------------------------------------------------------------------------------------------------------------------
// XML Generation API
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <utility>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
// XmlNode
//----------------------------------------------------------------------------------------------------------------------

class XmlNode
{
public:
    using Attribute = std::pair<std::string, std::string>;

    XmlNode();
    XmlNode(XmlNode* parent, std::string&& tag, std::vector<Attribute>&& attrs);
    XmlNode(XmlNode* parent, std::string&& tag, std::vector<Attribute>&& attrs, std::string text);
    XmlNode(XmlNode&& node);
    func operator= (XmlNode&& node) -> XmlNode&;

    ~XmlNode();

    func tag(std::string&& tagName, std::vector<std::pair<std::string, std::string>>&& attrs, 
        XmlNode** outRef = nullptr) -> XmlNode&;
    func text(std::string&& tagName, std::vector<Attribute>&& attrs, std::string&& text) -> XmlNode&;
    func end() -> XmlNode&;

    func generate() const -> std::string;

private:
    func buildXml(std::string& str, int indent) const -> void;

private:
    std::string m_tag;
    std::vector<Attribute> m_attrs;
    std::string m_text;
    std::vector<XmlNode*> m_nodes;
    XmlNode* m_parent;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
