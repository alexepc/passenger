#  Phusion Passenger - https://www.phusionpassenger.com/
#  Copyright (c) 2014-2015 Phusion
#
#  "Phusion Passenger" is a trademark of Hongli Lai & Ninh Bui.
#
#  See LICENSE file for license information.

require 'optparse'
require 'net/http'
require 'socket'
PhusionPassenger.require_passenger_lib 'constants'
PhusionPassenger.require_passenger_lib 'admin_tools/instance_registry'
PhusionPassenger.require_passenger_lib 'config/command'
PhusionPassenger.require_passenger_lib 'config/utils'
PhusionPassenger.require_passenger_lib 'utils/json'

module PhusionPassenger
  module Config

    class ReopenLogsCommand < Command
      include PhusionPassenger::Config::Utils

      def run
        parse_options
        select_passenger_instance
        perform_reopen_logs
      end

    private
      def self.create_option_parser(options)
        OptionParser.new do |opts|
          nl = "\n" + ' ' * 37
          opts.banner = "Usage: passenger-config reopen-logs [OPTIONS]\n"
          opts.separator ""
          opts.separator "  Instruct #{PROGRAM_NAME} agent processes to reopen their log files. This"
          opts.separator "  should be invoked after you've rotated log files. This command returns after"
          opts.separator "  the log files have been reopened."
          opts.separator ""

          opts.separator "Options:"
          opts.on("--ignore-logs-not-available", "Exit successfully if #{PROGRAM_NAME}#{nl}" +
            "was not configured with a log file") do
            options[:ignore_logs_not_available] = true
          end
          opts.on("--instance NAME", String, "The #{PROGRAM_NAME} instance to select") do |value|
            options[:instance] = value
          end
          opts.on("-h", "--help", "Show this help") do
            options[:help] = true
          end
        end
      end

      def perform_reopen_logs
        perform_reopen_logs_on("watchdog", "watchdog_api")
        perform_reinherit_logs_on("core", "core_api")
        perform_reinherit_logs_on("UstRouter", "ust_router_api")
        puts "All done"
      end

      def perform_reopen_logs_on(name, socket_name)
        puts "Reopening logs for #{PROGRAM_NAME} #{name}"
        request = Net::HTTP::Post.new("/reopen_logs.json")
        try_performing_full_admin_basic_auth(request, @instance)
        request.content_type = "application/json"
        response = @instance.http_request("agents.s/#{socket_name}", request)
        if response.code.to_i == 401
          print_full_admin_command_permission_error
          abort
        elsif response["content-type"] == "application/json"
          if response.code.to_i / 100 != 2
            handle_error(name, response)
          end
        else
          STDERR.puts "*** An error occured while communicating with the #{AGENT_EXE} #{name} (code #{response.code}):"
          STDERR.puts response.body
          abort
        end
      end

      def perform_reinherit_logs_on(name, socket_name)
        puts "Reopen logs for #{AGENT_EXE} #{name} (through reinheritance)"
        request = Net::HTTP::Post.new("/reinherit_logs.json")
        try_performing_full_admin_basic_auth(request, @instance)
        request.content_type = "application/json"
        response = @instance.http_request("agents.s/#{socket_name}", request)
        if response.code.to_i == 401
          print_full_admin_command_permission_error
          abort
        elsif response["content-type"] == "application/json"
          if response.code.to_i / 100 != 2
            handle_error(name, response)
          end
        else
          STDERR.puts "*** An error occured while communicating with the #{AGENT_EXE} #{name} (code #{response.code}):"
          STDERR.puts response.body
          abort
        end
      end

      def handle_error(name, response)
        json = PhusionPassenger::Utils::JSON.parse(response.body)
        if !should_ignore_error?(json)
          STDERR.puts "*** An error occured while communicating with the #{AGENT_EXE} #{name} (code #{response.code}):"
          STDERR.puts json['message']
          abort
        end
      end

      def should_ignore_error?(json)
        return @options[:ignore_logs_not_available] && json["code"] == "NO_LOG_FILE"
      end
    end

  end # module Config
end # module PhusionPassenger
