#include <string_view>

constexpr auto schema = std::string_view{ R"(
{
  "type": "object",
  "properties": {
    "config_server": {
      "type": "object",
      "properties": {
        "port": {
          "type": "integer",
          "minimum": 0,
          "maximum": 65535
        },
        "api_key": {
          "type": "string"
        }
      },
      "required": [
        "port",
        "api_key"
      ]
    },
    "logging": {
      "type": "object",
      "properties": {
        "level": {
          "type": "string",
          "enum": [
            "trace",
            "debug",
            "info",
            "warning",
            "error",
            "critical"
          ]
        },
        "output_mode": {
          "type": "string",
          "enum": [
            "file",
            "console",
            "both"
          ]
        },
        "file_name": {
          "type": "string"
        },
        "max_file_size": {
          "type": "integer",
          "minimum": 10,
          "maximum": 500
        },
        "max_files": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10
        }
      },
      "if": {
        "properties": {
          "output_mode": {
            "enum": [
              "console"
            ]
          }
        }
      },
      "then": {
        "required": [
          "level",
          "output_mode"
        ]
      },
      "else": {
        "required": [
          "level",
          "output_mode",
          "file_name",
          "max_file_size",
          "max_files"
        ]
      }
    },
    "smpp_gateway": {
      "type": "object",
      "properties": {
        "prometheus": {
          "type": "object",
          "properties": {
            "address": {
              "type": "string"
            },
            "labels": {
              "type": "array",
              "items": {
                  "type": "object",
                  "properties": {
                    "key": {
                      "type": "string"
                    },
                    "value": {
                      "type": "string"
                    }
                  },
                  "required": [
                    "key",
                    "value"
                  ]
              },
              "uniqueItems": true
            }
          },
          "required": [
            "address",
            "labels"
          ]
        },
        "policy": {
          "type": "object",
          "properties": {
            "name": {
              "type": "string"
            },
            "address": {
              "type": "array",
              "items": {
                "type": "string"
              }
            },
            "timeout": {
              "type": "integer",
              "minimum": 0
            }
          },
          "required": [
            "name",
            "address",
            "timeout"
          ]
        },
        "logger": {
          "type": "object",
          "properties": {
            "ao_logger": {
              "type": "object",
              "properties": {
                "enabled": {
                  "type": "boolean"
                },
                "file_mode": {
                  "type": "string",
                  "enum": [
                    "text",
                    "binary"
                  ]
                },
                "file_name_format": {
                  "type": "string"
                },
                "create_path": {
                  "type": "string"
                },
                "close_path": {
                  "type": "string"
                },
                "buffer_size": {
                  "type": "integer"
                },
                "records_threshold": {
                  "type": "integer"
                },
                "time_threshold": {
                  "type": "integer"
                }
              },
              "required": [
                "enabled",
                "file_mode",
                "file_name_format",
                "create_path",
                "close_path",
                "buffer_size",
                "records_threshold",
                "time_threshold"
              ]
            },
            "at_logger": {
              "type": "object",
              "properties": {
                "enabled": {
                  "type": "boolean"
                },
                "file_mode": {
                  "type": "string",
                  "enum": [
                    "text",
                    "binary"
                  ]
                },
                "file_name_format": {
                  "type": "string"
                },
                "create_path": {
                  "type": "string"
                },
                "close_path": {
                  "type": "string"
                },
                "buffer_size": {
                  "type": "integer"
                },
                "records_threshold": {
                  "type": "integer"
                },
                "time_threshold": {
                  "type": "integer"
                }
              },
              "required": [
                "enabled",
                "file_mode",
                "file_name_format",
                "create_path",
                "close_path",
                "buffer_size",
                "records_threshold",
                "time_threshold"
              ]
            },
            "dr_logger": {
              "type": "object",
              "properties": {
                "enabled": {
                  "type": "boolean"
                },
                "file_mode": {
                  "type": "string",
                  "enum": [
                    "text",
                    "binary"
                  ]
                },
                "file_name_format": {
                  "type": "string"
                },
                "create_path": {
                  "type": "string"
                },
                "close_path": {
                  "type": "string"
                },
                "buffer_size": {
                  "type": "integer"
                },
                "records_threshold": {
                  "type": "integer"
                },
                "time_threshold": {
                  "type": "integer"
                }
              },
              "required": [
                "enabled",
                "file_mode",
                "file_name_format",
                "create_path",
                "close_path",
                "buffer_size",
                "records_threshold",
                "time_threshold"
              ]
            },
            "reject_logger": {
              "type": "object",
              "properties": {
                "enabled": {
                  "type": "boolean"
                },
                "file_mode": {
                  "type": "string",
                  "enum": [
                    "text",
                    "binary"
                  ]
                },
                "file_name_format": {
                  "type": "string"
                },
                "create_path": {
                  "type": "string"
                },
                "close_path": {
                  "type": "string"
                },
                "buffer_size": {
                  "type": "integer"
                },
                "records_threshold": {
                  "type": "integer"
                },
                "time_threshold": {
                  "type": "integer"
                }
              },
              "required": [
                "enabled",
                "file_mode",
                "file_name_format",
                "create_path",
                "close_path",
                "buffer_size",
                "records_threshold",
                "time_threshold"
              ]
            },
            "tracer": {
              "type": "object",
              "properties": {
                "enabled": {
                  "type": "boolean"
                },
                "brokers": {
                  "type": "string"
                },
                "topic": {
                  "type": "string"
                }
              },
              "required": [
                "enabled",
                "brokers",
                "topic"
              ]
            }
          },
          "required": [
            "ao_logger",
            "at_logger",
            "dr_logger",
            "reject_logger",
            "tracer"
          ]
        },
        "smpp_server": {
          "type": "object",
          "properties": {
            "ip": {
              "type": "string"
            },
            "port": {
              "type": "integer",
              "minimum": 1024,
              "maximum": 65535
            },
            "system_id": {
              "type": "string"
            },
            "session_init_timeout": {
              "type": "integer"
            },
            "enquire_link_timeout": {
              "type": "integer"
            },
            "inactivity_timeout": {
              "type": "integer"
            },
            "external_client": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "system_id": {
                    "type": "string"
                  },
                  "permitted_bind_types": {
                    "type": "array",
                    "default": "TRX",
                    "items": {
                      "type": "string",
                      "enum": [
                        "TX",
                        "RX",
                        "TRX"
                      ]
                    },
                    "uniqueItems": true
                  },
                  "system_type": {
                    "type": "string"
                  },
                  "password": {
                    "type": "string"
                  },
                  "require_password_checking": {
                    "type": "boolean"
                  },
                  "require_ip_checking": {
                    "type": "boolean"
                  },
                  "ip_addresses": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    },
                    "uniqueItems": true
                  },
                  "ip_mask": {
                    "type": "string"
                  },
                  "submit_resp_msg_id_base": {
                    "type": "string",
                    "enum": [
                      "HEX",
                      "DEC",
                      "hex",
                      "dec"
                    ]
                  },
                  "delivery_report_msg_id_base": {
                    "type": "string",
                    "enum": [
                      "HEX",
                      "DEC",
                      "hex",
                      "dec"
                    ]
                  },
                  "ignore_user_validity_period": {
                    "type": "boolean"
                  },
                  "submit_validity_period": {
                    "type": "integer"
                  },
                  "delivery_report_validity_period": {
                    "type": "integer"
                  },
                  "dialog_timeout":{
                    "type":"integer",
                    "minimum":0,
                    "maximum":1000
                  },
                  "status_report_state_generator": {
                    "type": "boolean"
                  },
                  "status_report_state": {
                    "type": "string",
                    "enum": [
                      "never",
                      "always",
                      "on_failed",
                      "on_succeed"
                    ]
                  },
                  "source_address_check": {
                    "type": "boolean"
                  },
                  "source_ton_npi_check": {
                    "type": "boolean"
                  },
                  "destination_address_check": {
                    "type": "boolean"
                  },
                  "destination_ton_npi_check": {
                    "type": "boolean"
                  },
                  "dcs_check": {
                    "type": "boolean"
                  },
                  "black_white_check": {
                    "type": "boolean"
                  },
                  "max_session": {
                    "type": "integer",
                    "minimum":0,
                    "maximum":50
                  },
                  "receive_flow_control":{
                    "type":"object",
                    "properties":{
                      "flow_method":{
                        "type":"string",
                        "enum":[
                          "disabled",
                          "fixed_flow",
                          "normal",
                          "adaptive",
                          "credit",
                          "limit_credit"
                        ]
                      },
                      "max_packets_per_second":{
                        "type":"integer"
                      },
                      "should_reject_packet":{
                        "type":"boolean"
                      },
                      "credit_windows_size":{
                        "type":"integer"
                      },
                      "max_slippage":{
                        "type":"integer"
                      }
                    }
                  },
                  "send_flow_control":{
                    "type":"object",
                    "properties":{
                      "flow_method":{
                        "type":"string",
                        "enum":[
                          "disabled",
                          "fixed_flow",
                          "normal",
                          "adaptive",
                          "credit",
                          "limit_credit"
                        ]
                      },
                      "max_packets_per_second":{
                        "type":"integer"
                      },
                      "credit_windows_size":{
                        "type":"integer"
                      },
                      "max_slippage":{
                        "type":"integer"
                      }
                    }
                  }
                },
                "required": [
                  "system_id",
                  "permitted_bind_types",
                  "system_type",
                  "password",
                  "require_password_checking",
                  "submit_resp_msg_id_base",
                  "delivery_report_msg_id_base",
                  "ignore_user_validity_period",
                  "submit_validity_period",
                  "delivery_report_validity_period",
                  "dialog_timeout",
                  "status_report_state_generator",
                  "status_report_state",
                  "source_address_check",
                  "source_ton_npi_check",
                  "destination_address_check",
                  "destination_ton_npi_check",
                  "dcs_check",
                  "black_white_check",
                  "receive_flow_control",
                  "send_flow_control"
                ]
              },
              "uniqueItems": true
            },
            "routing": {
              "type": "object",
              "properties": {
                "reverse": {
                  "type": "boolean"
                },
                "routes": {
                  "type": "array",
                  "items": {
                    "type": "object",
                    "properties": {
                      "id": {
                        "type": "integer"
                      },
                      "priority": {
                        "type": "integer"
                      },
                      "from": {
                        "type": "string"
                      },
                      "source_address": {
                        "type": "string"
                      },
                      "destination_address": {
                        "type": "string"
                      },
                      "pdu_type": {
                        "type": "string",
                        "enum": [
                          "",
                          "*",
                          "submit",
                          "deliver",
                          "delivery_report"
                        ]
                      },
                      "target": {
                        "type": "string"
                      }
                    },
                    "required": [
                      "id",
                      "priority",
                      "from",
                      "source_address",
                      "destination_address",
                      "pdu_type",
                      "target"
                    ]
                  },
                  "uniqueItems": true
                }
              },
              "required": [
                "reverse",
                "routes"
              ]
            }
          },
          "required": [
            "ip",
            "port",
            "system_id",
            "session_init_timeout",
            "enquire_link_timeout",
            "inactivity_timeout",
            "external_client",
            "routing"
          ]
        },
        "network_interface": {
          "type": "object",
          "properties": {
            "name": {
              "type": "string"
            },
            "address": {
              "type": "array",
              "items": {
                "type": "string"
              }
            },
            "timeout": {
              "type": "integer",
              "minimum": 0
            },
            "router": {
              "type": "object",
              "properties": {
                "reverse": {
                  "type": "boolean"
                },
                "routing_list": {
                  "type": "array",
                  "items": {
                    "type": "object",
                    "properties": {
                      "msg_type": {
                        "type": "string",
                        "enum": [
                          "submit"
                        ]
                      },
                      "method": {
                        "type": "string",
                        "enum": [
                          "prefix",
                          "client_id",
                          "rond_robin",
                          "load_balance",
                          "broadcast"
                        ]
                      },
                      "routes": {
                      "type": "array",
                      "items": {
                        "type": "object",
                        "properties": {
                          "id": {
                            "type": "string"
                          },
                          "prefix": {
                            "type": "array",
                            "items": {
                              "type": "string"
                            }
                          },
                          "target": {
                            "type": "string"
                          }
                        },
                        "required": [
                          "id",
                          "prefix",
                          "target"
                        ]
                      }
                    }
                    },
                    "required": [
                      "msg_type",
                      "method"
                    ]
                  },
                  "uniqueItems": true
                }
              },
              "required": [
                "reverse",
                "routing_list"
              ]
            }
          },
          "required": [
            "name",
            "address",
            "timeout",
            "router"
          ]
        }
      },
      "required": [
        "prometheus",
        "policy",
        "logger",
        "smpp_server",
        "network_interface"
      ]
    }
  },
  "required": [
    "config_server",
    "logging",
    "smpp_gateway"
  ]
}
)" };
