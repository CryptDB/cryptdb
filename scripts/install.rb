#!/usr/bin/env ruby

require 'etc'
require 'fileutils'

$usage =
    "Usage: ./install <path-to-cryptdb> [automake-version] [gcc-version]"

SHADOW_NAME = "shadow"
PROXY_NAME = "proxy-src"
MYSQL_NAME = "mysql-src"
TAR_GZ = ".tar.gz"

class String
    def cyan
        "\033[36m#{self}\033[0m"
    end

    def red
        "\033[31m#{self}\033[0m"
    end

    def bold
        "\033[1m#{self}\033[22m"
    end
end

# NOTE: For whatever reason we fail to get the correct exit code when
# apt-get fails; and execution continues per-normal.
def get_pkgs
    p_puts "Retrieving packages..."

    pkg_shell = ShellDoer.new("~")
    pkg_shell.>(%q{
        sudo apt-get install gawk liblua5.1-0-dev libntl-dev         \
                libmysqlclient-dev libssl-dev libbsd-dev        \
                libevent-dev libglib2.0-dev libgmp-dev          \
                mysql-server libaio-dev automake                \
                gtk-doc-tools flex cmake libncurses5-dev        \
                bison g++ make
    })
end

def fn(cdb_path, in_make_v=nil, in_gcc_v=nil)
    cryptdb_path = File.expand_path(cdb_path)
    cryptdb_shell = ShellDoer.new(cryptdb_path)
    bins_path = File.join(cryptdb_path, "bins/")

    #############################
    #        mysql-proxy
    # ###########################
    # + automake fixups.
    p_puts "Checking automake..."

    automake_version = 
        if in_make_v
            in_make_v
        else
            first_line_version(%x(automake --version))
        end

    if automake_version.nil?
        fail no_version_fail("automake")
    end

    p_puts "Building mysql-proxy..."

    # untar
    proxy_path = File.join(cryptdb_path, PROXY_NAME)
    proxy_tar_path = File.join(bins_path, PROXY_NAME) + TAR_GZ
    cryptdb_shell.>("tar zxf #{proxy_tar_path}") 

    # automake compatibility fix
    # https://www.flameeyes.eu/autotools-mythbuster/forwardporting/automake.html
    mp_shell = ShellDoer.new(proxy_path)
    config_path = File.join(proxy_path, "configure.in")
    if Version.new(automake_version) >= Version.new("1.12")
        big = File.join(proxy_path, "big_configure.in")
        FileUtils.copy(big, config_path)
    else
        little = File.join(proxy_path, "little_configure.in")
        FileUtils.copy(little, config_path)
    end

    proxy_install_path = File.join(bins_path, "proxy-bin")
    mp_shell.>("./autogen.sh")
    mp_shell.>("./configure --enable-maintainer-mode --with-lua=lua5.1 --prefix=\"#{proxy_install_path}\"")
    mp_shell.>("make")
    mp_shell.>("make install")
    File.delete(config_path)
    mp_shell.>("rm -rf #{proxy_path}")

    #############################
    #            gcc
    #############################
    p_puts "Checking gcc..."

    gcc_version =
        if in_gcc_v
            in_gcc_v
        else
            first_line_version(%x(gcc --version))
        end

    if gcc_version.nil?
        fail no_version_fail("gcc")
    end

    if Version.new(gcc_version) < Version.new("4.6")
        fail("update your gcc version to >= 4.6 before installing!")
    end


    #############################
    #           mysql
    #############################
    p_puts "Building mysql..."

    # untar
    mysql_path = File.join(cryptdb_path, MYSQL_NAME)
    mysql_tar_path = File.join(bins_path, MYSQL_NAME) + TAR_GZ
    cryptdb_shell.>("tar zxf #{mysql_tar_path}")

    mysql_build_path = File.join(mysql_path, "/build")
    Dir.mkdir(mysql_build_path) if false == File.exists?(mysql_build_path)

    mysql_shell = ShellDoer.new(mysql_build_path)
    mysql_shell.>("cmake -DWITH_EMBEDDED_SERVER=on -DENABLE_DTRACE=off ..")
    mysql_shell.>("make")

    #############################
    #          cryptdb
    #############################
    p_puts "Building cryptdb..."

    conf_path = File.join(cryptdb_path, "conf", "config.mk")
    File.open(conf_path, 'w') do |file|
        file.write(generate_config(mysql_path))
    end
    cryptdb_shell.>("make clean")
    cryptdb_shell.>("make")
    cryptdb_shell.>("service mysql stop", true)
    cryptdb_shell.>("make install")
    cryptdb_shell.>("service mysql start")

    shadow_path = File.join(cryptdb_path, SHADOW_NAME)
    cryptdb_shell.>("rm -rf #{shadow_path}")
    cryptdb_shell.>("mkdir #{shadow_path}")

    # Give the user access to all the stuff we created.
    cryptdb_shell.>("chown -R #{Etc.getlogin} #{cryptdb_path}")
end

class ShellDoer
    def initialize(dir)
        @dir = dir
    end

    def >(cmd, ignore=false)
        pretty_execute(cmd, ignore)
    end

    private
    def pretty_execute(cmd, ignore)
        %x(cd #{@dir} && #{cmd.strip} 1>&2)
        if $?.exitstatus != 0 && false == ignore
            fail "`#{cmd}` failed".red.bold
        end
    end
end

def no_version_fail(name)
    "unable to determine #{name} version, supply version # thru command line argument.\n#{$usage}\n"
end

def first_line_version(text)
    /([0-9]+\.[0-9]+(?:\.[0-9]+)?)/.match(text.split("\n").first)[0]
end

# > Version numbers must have at least 1 number.
# > Only numbers and 'dot' are valid.
class Version < Array
    def initialize(s)
        if s.empty?
            fail "empty strings are not version numbers!"
        end

        parsed = parse_version(s)
        if parsed.empty?
            fail "unable to parse '#{s}' as version number"
        end
        super(parsed)
    end

    def >=(v2)
        fail "versions only compare with versions" if !v2.is_a?(Version)
        m = [self.size, v2.size].max
        (pad(self, m) <=> pad(v2, m)) >= 0
    end

    def <(v2)
        fail "versions only compare with versions" if !v2.is_a?(Version)
        m = [self.size, v2.size].max
        (pad(self, m) <=> pad(v2, m)) < 0
    end

    private
    def pad(a, size)
        return a if size < a.size
        a + [0] * (size - a.size)
    end

    def parse_version(v)
        v.scan(/([0-9]+)\.?/).map(&:first).map(&:to_i)
    end
end

def p_puts(output_me)
    puts output_me.cyan.bold
end

def generate_config(mysql_path)
    ["MYSRC := #{mysql_path}",
     "MYBUILD  := $(MYSRC)/build",
     "RPATH := 1",
     "",
     "## To enable debugging:",
     "# CXXFLAGS += -g",
     "",
     "## To use a different compiler:",
     "# CXX := g++-4.6",
     "",
     "## Where UDFs go:",
     "MYSQL_PLUGIN_DIR := /usr/lib/mysql/plugin"].join("\n")
end

def test_version
    pairs = [["1.1",        "1.3"],
             ["1.5",        "2.5"],
             ["1.12.2",     "2.3"],
             ["5",          "8.9"],
             ["1.0.0.1",    "1.1"],
             ["3.4.5",      "4.5.2"],
             ["2.0",        "5.1.0.0"],
             ["0.1",        "0.1.0.0.2"],
             ["0",          "0.1"]]

    pairs.inject(true) do |acc, (low, high)|
        Version.new(low) < Version.new(high) &&
        Version.new(high) >= Version.new(low) &&
        acc
    end
end

def root?
    if 0 != Process.uid
        fail "root required to install dependencies and UDFs".red.bold
    end
end

#############################
#############################
#   Execution Begins Here
#############################
#############################
if ARGV.size() < 1 || ARGV.size() > 3
    fail $usage
end

root?()
get_pkgs()
fn(ARGV[0], ARGV[1], ARGV[2])


#TODO: add restart of Mysql server after UDF updates
