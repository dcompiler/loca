module Plot

require 'fileutils' #instead of 'ftools'

	## Plots the curves for all available data and copy each file
	#  as zoom.file (complicated because file is called by path,
	#  i.e. directory/file is copied to directory/zoom.file).

	def self.rplot (currentDir, program, ext, zoom)
		if zoom == true
			arg1 = File.join(currentDir, "zoom_#{program}")
			arg2 = ext
			`Rscript #{LocaServerDir}/draw_dual.r #{arg1} #{arg2}`
		else
			arg1 = File.join(currentDir, program)
			arg2 = ext
			`Rscript #{LocaServerDir}/draw_dual.r #{arg1} #{arg2}`
		end
	 end

	def self.default (currentDir, program, ext)
		rplot(currentDir, program, ext, false)
		["fp", "lf", "mr", "rd"].each do |metric|
			FileUtils.cp(File.join( currentDir, \
									 program + ".#{metric}.png"), \
									 File.join( currentDir, \
									 "zoom_" + program + ".#{metric}.png"))
		end
	end	

	def self.zoom (currentDir, program, ext, xmin, xmax, metric)
		columnNum = (metric == "fp")? 1 : 2
		File.open(File.join( currentDir, "zoom_#{program}#{ext}"), "w") do |writefile|
			File.open(File.join( currentDir, "#{program}#{ext}"), "r") do |f|
				writefile.puts f.gets #print 1st line.
				if (metric == "fp")
					while line = f.gets # Print rest of lines where column 1 is in range
						if  (line.split[columnNum].to_f >= xmin) \
						and (line.split[columnNum].to_f <= xmax)
						    writefile.puts line
						end
					end
				else
					while line = f.gets # Print rest of lines where column 1 is in range
						if  (line.split[columnNum].to_f*64.0 >= xmin) \
						and (line.split[columnNum].to_f*64.0 <= xmax)
						    writefile.puts line
						end
					end
				end
			end
		end
		rplot(currentDir, program, ext, true)
	end

end