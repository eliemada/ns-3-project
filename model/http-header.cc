/**
 * @file http-header.cc
 * @brief Implementation file for HttpHeader class registration.
 *
 * This file registers the HttpHeader class with the ns-3 object system,
 * enabling runtime type identification and factory-based object creation.
 *
 * @author Elie Bruno
 * @date 2025
 */

#include "http-header.h"

namespace ns3 {

/**
 * @brief Register HttpHeader with the ns-3 type system.
 *
 * This macro ensures that HttpHeader is properly registered with ns-3's
 * object system at static initialization time, before main() executes.
 * This registration is required for:
 * - TypeId-based object creation
 * - Attribute system integration
 * - Trace source functionality
 */
NS_OBJECT_ENSURE_REGISTERED(HttpHeader);

} // namespace ns3
