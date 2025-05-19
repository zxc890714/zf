#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <string>
#include <sstream>
#include <iostream>

std::string createXMLContent() {
    // 创建XML文档
    rapidxml::xml_document<> doc;
    
    // 创建XML声明
    rapidxml::xml_node<>* decl = doc.allocate_node(rapidxml::node_declaration);
    decl->append_attribute(doc.allocate_attribute("version", "1.0"));
    decl->append_attribute(doc.allocate_attribute("encoding", "utf-8"));
    doc.append_node(decl);
    
    // 创建根节点
    rapidxml::xml_node<>* root = doc.allocate_node(rapidxml::node_element, "EventNotificationAlert");
    root->append_attribute(doc.allocate_attribute("xmlns", "http://www.isapi.org/ver20/XMLSchema"));
    root->append_attribute(doc.allocate_attribute("version", "2.0"));
    doc.append_node(root);
    
    // 添加子节点
    rapidxml::xml_node<>* ipAddress = doc.allocate_node(rapidxml::node_element, "ipAddress", "10.11.96.242");
    root->append_node(ipAddress);
    
    rapidxml::xml_node<>* portNo = doc.allocate_node(rapidxml::node_element, "portNo", "29999");
    root->append_node(portNo);
    
    rapidxml::xml_node<>* protocol = doc.allocate_node(rapidxml::node_element, "protocol", "HTTP");
    root->append_node(protocol);
    
    // 获取当前时间并格式化为ISO 8601格式
    std::string dateTime = "2025-05-19T11:36:00+08:00"; // 这里您可以使用实时时间
    rapidxml::xml_node<>* dateTimeNode = doc.allocate_node(rapidxml::node_element, "dateTime", 
                                                          doc.allocate_string(dateTime.c_str()));
    root->append_node(dateTimeNode);
    
    rapidxml::xml_node<>* activePostCount = doc.allocate_node(rapidxml::node_element, "activePostCount", "60");
    root->append_node(activePostCount);
    
    rapidxml::xml_node<>* eventType = doc.allocate_node(rapidxml::node_element, "eventType", "videoloss");
    root->append_node(eventType);
    
    rapidxml::xml_node<>* eventState = doc.allocate_node(rapidxml::node_element, "eventState", "inactive");
    root->append_node(eventState);
    
    rapidxml::xml_node<>* eventDescription = doc.allocate_node(rapidxml::node_element, "eventDescription", "videoloss");
    root->append_node(eventDescription);
    
    // 将XML转换为字符串
    std::stringstream ss;
    ss << doc;
    
    // 注意：ss.str()可能会在末尾有一个换行符
    std::string xml_content = ss.str();
    
    // 如果需要移除末尾的换行符
    if (!xml_content.empty() && xml_content.back() == '\n') {
        xml_content.pop_back();
    }
    
    return xml_content;
}

// 构建HTTP多部分请求
std::string createMultipartRequest(const std::string& xml_content) {
    std::string boundary = "MIME_boundary";
    
    // 构建请求体
    std::stringstream body_ss;
    body_ss << "--" << boundary << "\r\n";
    body_ss << "Content-Type: application/xml; charset=UTF-8\r\n";
    body_ss << "\r\n";  // 空行分隔头部和内容
    body_ss << xml_content << "\r\n";
    body_ss << "--" << boundary << "--\r\n";  // 结束分隔符
    
    std::string body = body_ss.str();
    size_t content_length = body.length();
    
    // 构建完整HTTP请求
    std::stringstream request_ss;
    request_ss << "POST /protocol/alarm-service/v1/listen?username=zhoufan14 HTTP/1.1\r\n";
    request_ss << "Host: 10.11.96.242:29999\r\n";
    request_ss << "Accept: */*\r\n";
    request_ss << "Content-Type: multipart/mixed; boundary=" << boundary << "\r\n";
    request_ss << "Content-Length: " << content_length << "\r\n";
    request_ss << "\r\n";  // 空行分隔头部和请求体
    request_ss << body;
    
    return request_ss.str();
}

int main() {
    // 创建XML内容
    std::string xml_content = createXMLContent();
    std::cout << "XML Content:\n" << xml_content << std::endl;
    std::cout << "XML Content Length: " << xml_content.length() << " bytes\n" << std::endl;
    
    // 创建多部分请求
    std::string request = createMultipartRequest(xml_content);
    std::cout << "Complete HTTP Request:\n" << request << std::endl;
    std::cout << "Total Request Length: " << request.length() << " bytes" << std::endl;
    
    // 这里您可以发送请求...
    
    return 0;
}
