//----------------------------------------------------------------------------------------------------------------------
// Implementation of XML Generation API
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <utils/xml.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
// Constructors and assignment operators

XmlNode::XmlNode()
    : m_parent(nullptr)
{

}

XmlNode::XmlNode(XmlNode&& node)
    : m_tag(move(node.m_tag))
    , m_attrs(move(node.m_attrs))
    , m_text(move(node.m_text))
    , m_nodes(move(node.m_nodes))
    , m_parent(nullptr)
{

}

func XmlNode::operator= (XmlNode&& node) -> XmlNode&
{
    m_tag = move(node.m_tag);
    m_attrs = move(node.m_attrs);
    m_text = move(node.m_text);
    m_nodes = move(node.m_nodes);
    m_parent = nullptr;
    return *this;
}

XmlNode::XmlNode(XmlNode* parent, std::string&& tag, std::vector<Attribute>&& attrs)
    : XmlNode(parent, move(tag), move(attrs), {})
{

}

XmlNode::XmlNode(XmlNode* parent, std::string&& tag, std::vector<Attribute>&& attrs, std::string text)
    : m_tag(move(tag))
    , m_attrs(move(attrs))
    , m_text(text)
    , m_nodes()
    , m_parent(parent)
{

}

//----------------------------------------------------------------------------------------------------------------------
// Destructor

XmlNode::~XmlNode()
{
    for (auto& node : m_nodes)
    {
        delete node;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// tag - create XML tag without text

func XmlNode::tag(std::string&& tagName, std::vector<std::pair<std::string, std::string>>&& attrs, 
    XmlNode** outRef /* = nullptr */) -> XmlNode&
{
    XmlNode* node = new XmlNode(this, move(tagName), move(attrs));
    m_nodes.push_back(node);
    if (outRef) *outRef = node;
    return *node;
}

//----------------------------------------------------------------------------------------------------------------------
// text - create XML tag with text

func XmlNode::text(std::string&& tagName, std::vector<Attribute>&& attrs, std::string&& text) -> XmlNode&
{
    XmlNode* node = new XmlNode(this, move(tagName), move(attrs), move(text));
    m_nodes.push_back(node);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------
// end - End the current tag

func XmlNode::end() -> XmlNode&
{
    return *m_parent;
}

//----------------------------------------------------------------------------------------------------------------------
// generate - Generate the output string

func XmlNode::generate() const -> string
{
    string str = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    for (const auto& node : m_nodes)
    {
        node->buildXml(str, 0);
    }
    return str;
}

//----------------------------------------------------------------------------------------------------------------------
// buildXml

func XmlNode::buildXml(std::string& str, int indent) const -> void
{
    string indentStr = string(indent, '\t');
    str += indentStr;

    str += "<" + m_tag;
    for (const auto& attr : m_attrs)
    {
        str += " " + attr.first + "=\"" + attr.second + "\"";
    }

    if (m_nodes.empty())
    {

        if (m_text.empty())
        {
            // XML line is <tag attr... />
            str += " />";
        }
        else
        {
            // XML line is <tag attr...>text</tag>
            str += ">" + m_text + "</" + m_tag + ">";
        }
    }
    else
    {
        // XML line is <tag>
        str += ">\n";
        for (const auto& node : m_nodes)
        {
            node->buildXml(str, indent + 1);
        }
        str += indentStr + "</" + m_tag + ">";
    }
    str += "\n";
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
