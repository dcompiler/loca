#!/usr/bin/ruby

tmp_dir = "/Users/cding/Temp"
# tmp_dir = "/tmp"

puts "Usage: #{$0} [rev]" if ARGV.size > 1

pk_dir = File.expand_path( File.dirname( __FILE__ ) )
repo_dir = File.join( tmp_dir, "loca" )

def run_cmd( cmd )
  puts cmd
  system cmd
end

run_cmd( "rm -rf #{repo_dir}" ) if File.exists?("/tmp/loca")

# create a local copy of the repository
run_cmd( "mkdir #{repo_dir}" )
run_cmd( "cd #{repo_dir}" )
Dir.chdir( repo_dir )
run_cmd( "hg init" )
run_cmd( "hg pull #{File.join( pk_dir, ".." )}" )

# update the files to the desired revision
if ARGV.size == 1
  rev = ARGV[0]
else
  # rev = run_cmd( "hg tip" ).split(":")[1].to_i
  rev = `hg tip`.split(":")[1].to_i
end
run_cmd( "hg up -r #{rev}" )

File.open( File.join( repo_dir, "doc", "version.txt" ), "w" ) do |f|
  f.puts "Locality Lab revision #{rev} created #{Time.now}"
end

# remove everything except for needed directories
needed = ["doc", "gem", "server"]

def remove( parent_dir, exceptions )
  Dir.new( parent_dir ).each do |file|
    next if [".", ".."].find_index( file ) != nil
    run_cmd( "rm -rf #{file}" ) unless exceptions.find_index( file ) != nil
  end
end

remove( repo_dir, needed )

# make a package
Dir.chdir( File.join(repo_dir, "..") )
run_cmd "tar -cf loca.tar #{repo_dir.split("/")[-1]}"
run_cmd "gzip #{File.join( repo_dir, '../loca.tar' )}"
run_cmd "rm -rf #{repo_dir}"
