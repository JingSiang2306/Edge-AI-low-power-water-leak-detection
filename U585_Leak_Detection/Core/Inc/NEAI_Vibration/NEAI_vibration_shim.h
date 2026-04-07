#ifndef INC_NEAI_VIBRATION_NEAI_VIBRATION_SHIM_H_
#define INC_NEAI_VIBRATION_NEAI_VIBRATION_SHIM_H_

#pragma once

/* Clear any previous mapping */
#ifdef neai_classification_init
#undef neai_classification_init
#endif
#ifdef neai_classification
#undef neai_classification
#endif
#ifdef neai_get_id
#undef neai_get_id
#endif
#ifdef neai_get_input_signal_size
#undef neai_get_input_signal_size
#endif
#ifdef neai_get_axis_number
#undef neai_get_axis_number
#endif
#ifdef neai_get_number_of_classes
#undef neai_get_number_of_classes
#endif
#ifdef neai_get_class_name
#undef neai_get_class_name
#endif

/* Map generic NEAI API names to VIBRATION-prefixed symbols produced by prefix_neai_lib.py
 * Example: neai_classification_init -> neai_vibration_neai_classification_init
 */
#define neai_classification_init     neai_vibration_neai_classification_init
#define neai_classification          neai_vibration_neai_classification

#define neai_get_id                  neai_vibration_neai_get_id
#define neai_get_input_signal_size   neai_vibration_neai_get_input_signal_size
#define neai_get_axis_number         neai_vibration_neai_get_axis_number
#define neai_get_number_of_classes   neai_vibration_neai_get_number_of_classes
#define neai_get_class_name          neai_vibration_neai_get_class_name

#endif /* INC_NEAI_VIBRATION_NEAI_VIBRATION_SHIM_H_ */
