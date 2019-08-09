#!/usr/bin/ruby

if (ARGV.length != 1)
  puts "USAGE: inputTraceFile"
  exit
end

inputFileName = ARGV[0]

parts = inputFileName.split('_')
testcase_name = parts[0]
suffix = parts[2]

revFileName = testcase_name+"_reverse_"+suffix
#revFile = File.new(revFileName, "w")
#revFile.puts File.open(inputFileName).readlines.reverse
#revFile.close
cmd = "./reverse_file #{inputFileName} #{revFileName}"
puts cmd
system cmd

disFileName = testcase_name+"_rev-OPT-dis_"+suffix
cmd = "time -p ./OPTStackDistanceAnalyzer #{revFileName} #{disFileName}"
puts cmd
system cmd

finalFileName = testcase_name+"_forward-OPT-dis_"+suffix
#finalFile = File.new(finalFileName, "w")
#finalFile.puts File.open(disFileName).readlines.reverse
#finalFile.close
puts "./reverse_file #{disFileName} #{finalFileName}"
system "./reverse_file #{disFileName} #{finalFileName}"

# system "rm -f #{revFileName} #{disFileName}"
